/*
	DurandalGL.mm — Durandal project

	See DurandalGL.h. The OpenGL 1.x subset over Metal, the Metal display
	(CAMetalLayer, frames, presentation), and pass-through to real OpenGL
	when Metal display mode is off.

	Semantics follow the OpenGL 2.1 specification for everything the 2D
	code relies on: vertices are transformed on the CPU (modelview,
	projection, texture matrices; user clip planes in eye space), face
	culling is decided in window space as OpenGL does, polygons are fanned,
	and the Metal pipelines reproduce blending, alpha test, colour masks,
	stencil and the XOR logic op.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

// Apple's frameworks first (see DurandalMetal.mm)
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#if !__has_feature(objc_arc)
#error "DurandalGL.mm must be compiled with ARC"
#endif

#include "DurandalGL.h"
#include "DurandalMetal.h"
#include "DurandalPreferences.h"
#include "DurandalBenchmark.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_metal.h>

#include "cseries.h"
#include "Logging.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Shaders for the 2D subset and the world composite

static const char* kDGLShaderSource = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct GLVertex {
	float4 position;
	float4 color;
	float2 texcoord;
	float2 pad;
	float clip[6];
	float pad2[2];
};

struct GLOut {
	float4 position [[position]];
	float4 color [[user(color)]];
	float2 texcoord [[user(texcoord)]];
	float clip [[clip_distance]] [6];
};

struct GLIn {
	float4 position [[position]];
	float4 color [[user(color)]];
	float2 texcoord [[user(texcoord)]];
};

struct FragUniforms {
	int textured;
	int env_mode;		// 0 modulate, 1 replace
	int alpha_func;		// 0 always, 1 never, 2 less, 3 lequal, 4 equal, 5 gequal, 6 greater, 7 notequal
	float alpha_ref;
};

vertex GLOut gl_vertex(const device GLVertex* v [[buffer(0)]], uint vid [[vertex_id]])
{
	GLOut o;
	const float4 p = v[vid].position;
	// OpenGL clip-space depth [-w, w] to Metal's [0, w]
	o.position = float4(p.x, p.y, (p.z + p.w) * 0.5, p.w);
	o.color = v[vid].color;
	o.texcoord = v[vid].texcoord;
	for (int i = 0; i < 6; ++i)
		o.clip[i] = v[vid].clip[i];
	return o;
}

static bool alpha_passes(constant FragUniforms& u, float a)
{
	switch (u.alpha_func) {
		case 1: return false;
		case 2: return a < u.alpha_ref;
		case 3: return a <= u.alpha_ref;
		case 4: return a == u.alpha_ref;
		case 5: return a >= u.alpha_ref;
		case 6: return a > u.alpha_ref;
		case 7: return a != u.alpha_ref;
		default: return true;
	}
}

static float4 shade(GLIn in, constant FragUniforms& u, texture2d<float> tex, sampler smp)
{
	float4 c = in.color;
	if (u.textured) {
		const float4 t = tex.sample(smp, in.texcoord);
		c = (u.env_mode == 1) ? t : c * t;
	}
	return c;
}

fragment float4 gl_fragment(GLIn in [[stage_in]], constant FragUniforms& u [[buffer(0)]],
							texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]])
{
	const float4 c = shade(in, u, tex, smp);
	if (!alpha_passes(u, c.a))
		discard_fragment();
	return c;
}

// GL_COLOR_LOGIC_OP with GL_XOR (the static fade): programmable blending
fragment float4 gl_xor_fragment(GLIn in [[stage_in]], constant FragUniforms& u [[buffer(0)]],
								texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]],
								float4 dst [[color(0)]])
{
	const float4 c = shade(in, u, tex, smp);
	if (!alpha_passes(u, c.a))
		discard_fragment();
	const uint4 s = uint4(round(saturate(c) * 255.0));
	const uint4 d = uint4(round(saturate(dst) * 255.0));
	return float4(s ^ d) / 255.0;
}

struct BlitOut {
	float4 position [[position]];
	float2 uv;
};

vertex BlitOut world_blit_vertex(uint vid [[vertex_id]])
{
	const float2 corners[4] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1) };
	BlitOut o;
	o.position = float4(corners[vid], 0.0, 1.0);
	o.uv = float2(corners[vid].x * 0.5 + 0.5, 0.5 - corners[vid].y * 0.5);
	return o;
}

// Final pass: canvas -> drawable. For the EDR layer, decode the canvas's
// sRGB-encoded values to linear light (exact sRGB curve), so SDR white stays
// SDR white; the optional test patch shows 1.0 beside the display's headroom.
struct OutputParams {
	int decode;
	float test_headroom;
	float2 size;
	int glow;			// glow image bound (E1)
	float glow_gain;	// EDR: how far above SDR white glow reaches
	int bloom;			// bloom image bound (E1)
	float bloom_strength;
	float4 glow_rect;	// the world's rectangle in the canvas (x, y, w, h), top-left origin
	float4 grade;		// scene grade: brightness offset, contrast, gamma, 1 when in force
	float4 heat;		// heat shimmer: x on, y seconds
};

static float3 linear_to_srgb(float3 c)
{
	return select(1.055 * pow(c, float3(1.0 / 2.4)) - 0.055, c * 12.92, c <= 0.0031308);
}

static float3 srgb_to_linear(float3 c)
{
	return select(pow((c + 0.055) / 1.055, float3(2.4)), c / 12.92, c <= 0.04045);
}

fragment float4 output_fragment(BlitOut in [[stage_in]], constant OutputParams& p [[buffer(0)]],
								texture2d<float> tex [[texture(0)]], texture2d<float> glow [[texture(1)]],
								texture2d<float> bloom [[texture(2)]])
{
	constexpr sampler s(filter::nearest, address::clamp_to_edge);
	float4 canvas = tex.sample(s, in.uv);
	// Heat shimmer (Rampant): where the bloom is warm (the air over lava),
	// the world image wavers, rising; the HUD and overlays stay still
	if (p.heat.x > 0.0 && p.bloom && canvas.a < 0.5) {
		const float2 g = (in.uv * p.size - p.glow_rect.xy) / p.glow_rect.zw;
		if (all(g >= 0.0) && all(g <= 1.0)) {
			constexpr sampler hs(filter::linear, address::clamp_to_edge);
			const float3 b = max(bloom.sample(hs, g).rgb, float3(0.0));
			const float heat = saturate((b.r - b.b * 1.2) * 1.5);
			if (heat > 0.01) {
				const float2 px = in.uv * p.size;
				const float t = p.heat.y;
				const float2 wave = float2(sin(px.y * 0.09 + t * 7.0 + sin(px.x * 0.031 + t)),
										   cos(px.x * 0.07 + px.y * 0.05 + t * 5.3));
				const float2 moved = in.uv + wave * heat * 2.5 / p.size;
				const float4 there = tex.sample(s, moved);
				if (there.a < 0.5)
					canvas = there;
			}
		}
	}
	float3 rgb = canvas.rgb;
	// Scene grade (brightness, contrast, gamma) on the world image only:
	// the world marks its pixels with alpha 0, overlays raise it
	if (p.grade.w > 0.0) {
		const float3 graded = pow(saturate((rgb - 0.5) * p.grade.y + 0.5 + p.grade.x), float3(1.0 / p.grade.z));
		rgb = mix(rgb, graded, 1.0 - canvas.a);
	}
	if (p.decode)
		rgb = srgb_to_linear(rgb);
	// Glow and bloom (E1), in linear light, where the world is still
	// visible: the world image marks its pixels with alpha 0; the HUD,
	// fades and overlays drawn on top raise it (as their own coverage).
	// Glow shows only above SDR white (EDR); bloom shows in both.
	if (p.glow || p.bloom) {
		const float2 g = (in.uv * p.size - p.glow_rect.xy) / p.glow_rect.zw;
		const float cover = 1.0 - canvas.a;
		if (all(g >= 0.0) && all(g <= 1.0) && cover > 0.0) {
			constexpr sampler gs(filter::linear, address::clamp_to_edge);
			float3 lin = p.decode ? rgb : srgb_to_linear(rgb);
			if (p.bloom)
				lin += max(bloom.sample(gs, g).rgb, float3(0.0)) * p.bloom_strength * cover;
			if (p.glow && p.decode)
				lin += srgb_to_linear(saturate(glow.sample(gs, g).rgb)) * p.glow_gain * cover;
			rgb = p.decode ? lin : linear_to_srgb(saturate(lin));
		}
	}
	if (p.test_headroom > 0.0) {
		const float2 px = in.uv * p.size;
		if (px.y < 96.0 && px.x > p.size.x - 192.0)
			rgb = (px.x > p.size.x - 96.0) ? float3(p.test_headroom) : float3(1.0);
	}
	return float4(rgb, 1.0);
}

// The world image into the canvas. With glow, its pixels get alpha 0, so
// the output pass knows where the world is still uncovered.
struct WorldBlitParams {
	float gamma;
	int mark_world;
	float distortion;	// underwater (W1)
	float time;
	float ao_far;		// ambient shadows (Round 12): reach; 0 none
	float ao_view;		//   1: show the occlusion (development)
	float2 ao_texel;	//   1 / the occlusion image's size
	float shade;		// distance shade (Round 12): 0..1
	float shade_pad[3];
};

fragment float4 world_blit_fragment(BlitOut in [[stage_in]], constant WorldBlitParams& p [[buffer(0)]],
									texture2d<float> tex [[texture(0)]], texture2d<float> ao [[texture(1)]],
									texture2d<float> dist [[texture(2)]])
{
	constexpr sampler s(filter::nearest, address::clamp_to_edge);
	float2 uv = in.uv;
	if (p.distortion > 0.0)
		uv += p.distortion * 0.002 * float2(sin(uv.y * 23.0 + p.time * 1.05), cos(uv.x * 17.0 + p.time * 0.85));
	float4 c = tex.sample(s, uv);
	if (p.ao_far > 0.0) {
		// Ambient shadows: a 3x3 depth-aware blur of the half-resolution
		// occlusion (bilinear taps), faded out over the last quarter of
		// its reach so haze and far halls are left alone
		constexpr sampler smooth(filter::linear, address::clamp_to_edge);
		const float d0 = dist.sample(s, uv).r;
		float a = 1.0;
		if (d0 < p.ao_far) {
			float sum = 0.0, weight = 0.0;
			const float tolerance = 0.05 * d0 + 32.0;
			for (int y = -1; y <= 1; ++y)
				for (int x = -1; x <= 1; ++x) {
					const float2 t = uv + float2(x, y) * p.ao_texel;
					const float d = dist.sample(s, t).r;
					const float w = exp(-abs(d - d0) / tolerance);
					sum += ao.sample(smooth, t).r * w;
					weight += w;
				}
			a = weight > 0.0 ? sum / weight : 1.0;
			a = mix(1.0, a, saturate((p.ao_far - d0) / (0.25 * p.ao_far)));
		}
		if (p.ao_view > 0.5)
			return float4(a, a, a, p.mark_world ? 0.0 : 1.0);
		c.rgb *= a;
	}
	if (p.shade > 0.0) {
		// Distance shade: beyond the headlight's reach surfaces darken with
		// distance, up to 70% of the way at full strength; the sky (at
		// the far clear value) is left alone
		const float d = dist.sample(s, uv).r;
		if (d < 1.0e6)
			c.rgb *= 1.0 - 0.7 * p.shade * smoothstep(8.0 * 1024.0, 48.0 * 1024.0, d);
	}
	return float4(pow(saturate(c.rgb), float3(p.gamma)), p.mark_world ? 0.0 : 1.0);
}
)MSL";

// ---------------------------------------------------------------------------

float DurandalEDR_Headroom(SDL_Window* window);	// DurandalEDR.mm
float DurandalEDR_RefreshRate(SDL_Window* window);	// DurandalEDR.mm

namespace {

struct GLVertex {
	simd_float4 position;
	simd_float4 color;
	simd_float2 texcoord;
	simd_float2 pad;
	float clip[6];
	float pad2[2];
};
static_assert(sizeof(GLVertex) == 80, "GLVertex layout");

struct FragUniforms {
	int textured;
	int env_mode;
	int alpha_func;
	float alpha_ref;
};

struct Texture {
	id<MTLTexture> texture;
	int width = 0, height = 0;
	bool srgb = false;
	bool alpha_opaque = false;	// RGB internal formats sample alpha as 1
	GLenum min_filter = GL_NEAREST_MIPMAP_LINEAR;	// GL defaults
	GLenum mag_filter = GL_LINEAR;
	GLenum wrap_s = GL_REPEAT, wrap_t = GL_REPEAT;
	float anisotropy = 1;
	bool generate_mipmap = false;
};

struct ClientArray {
	bool enabled = false;
	GLint size = 4;
	GLenum type = GL_FLOAT;
	GLsizei stride = 0;
	const GLvoid* pointer = nullptr;
};

struct State {
	// enables
	bool blend = false, alpha_test = false, texture_2d = false, scissor_test = false,
		stencil_test = false, cull_face = false, depth_test = false, fog = false, logic_op = false;
	bool clip_enabled[6] = {false, false, false, false, false, false};
	simd_float4 clip_equation[6];	// eye space
	// colour buffer
	GLenum blend_src = GL_ONE, blend_dst = GL_ZERO;
	GLenum alpha_func = GL_ALWAYS;
	float alpha_ref = 0;
	GLenum logic_opcode = GL_COPY;
	bool color_mask[4] = {true, true, true, true};
	simd_float4 clear_color = {0, 0, 0, 0};
	// current
	simd_float4 color = {1, 1, 1, 1};
	// polygon
	GLenum cull_mode = GL_BACK, front_face = GL_CCW;
	// viewport / scissor
	GLint viewport[4] = {0, 0, 0, 0};
	GLint scissor_box[4] = {0, 0, 0, 0};
	// stencil
	GLenum stencil_func = GL_ALWAYS;
	GLint stencil_ref = 0;
	GLuint stencil_value_mask = 0xFF, stencil_write_mask = 0xFF;
	GLenum stencil_fail = GL_KEEP, stencil_zfail = GL_KEEP, stencil_zpass = GL_KEEP;
	GLint clear_stencil = 0;
	// texture
	GLuint bound_texture = 0;
	GLenum env_mode = GL_MODULATE;
	// transform
	GLenum matrix_mode = GL_MODELVIEW;
};

struct AttribFrame {
	GLbitfield mask;
	State state;
};

// ---- Metal objects
bool wanted_decided = false;
bool wanted = false;
bool active = false;
SDL_Window* window = nullptr;
SDL_MetalView metal_view = nullptr;
CAMetalLayer* layer = nil;
id<MTLDevice> device;
id<MTLCommandQueue> queue;
id<MTLLibrary> library;
id<MTLRenderPipelineState> world_blit_pipeline;
std::unordered_map<uint64_t, id<MTLRenderPipelineState>> pipelines;
std::unordered_map<uint64_t, id<MTLDepthStencilState>> stencil_states;
std::unordered_map<uint64_t, id<MTLSamplerState>> samplers;
id<MTLTexture> white_texture;
id<MTLSamplerState> default_sampler;

// ---- Frame
id<CAMetalDrawable> drawable;
// Everything draws into this 8-bit canvas exactly as OpenGL would into its
// back buffer; Present() then writes it to the drawable, decoding to linear
// light for the EDR (half-float) layer when HDR output is on.
id<MTLTexture> canvas;
bool offscreen = false;		// DURANDAL_BENCHMARK_OFFSCREEN: render, never present
bool vsync_on = true;
bool edr_on = false;
bool edr_test = false;		// DURANDAL_EDR_TEST=1: HDR test patch, top right
id<MTLRenderPipelineState> output_pipeline_sdr, output_pipeline_edr;
// Presentation timing for the benchmark
std::mutex present_mutex;
std::vector<double> present_times;
bool collect_present_times = false;
dispatch_semaphore_t frames_in_flight;
const long kMaxFramesInFlight = 2;

// Drawables in the swap chain: three. With vsync and only two, the CPU
// waits for the drawable the display holds and the GPU idles, so an 8 ms
// frame at 240 Hz reached the screen every third refresh (69 fps on a recorded
// film); with three it reached every refresh (202 fps), at most one
// refresh later. DURANDAL_DRAWABLES=2|3 overrides for development runs.
NSUInteger drawable_count(bool vsync)
{
	if (const char* d = getenv("DURANDAL_DRAWABLES"))
		return NSUInteger(std::max(2, std::min(3, atoi(d))));
	(void)vsync;
	return 3;
}
id<MTLTexture> stencil_texture;
id<MTLCommandBuffer> command_buffer;
id<MTLRenderCommandEncoder> encoder;
bool target_cleared = false;
int target_width = 0, target_height = 0;

// Vertex memory: 1 MB shared buffers, reused once the frame using them completes
std::mutex arena_mutex;
std::vector<id<MTLBuffer>> free_buffers;
std::vector<id<MTLBuffer>> frame_buffers;
id<MTLBuffer> current_buffer;
size_t current_offset = 0;
const size_t kArenaSize = 1 << 20;

// ---- GL state
State st;
std::vector<AttribFrame> attrib_stack;
std::vector<simd_float4x4> modelview_stack(1, matrix_identity_float4x4);
std::vector<simd_float4x4> projection_stack(1, matrix_identity_float4x4);
std::vector<simd_float4x4> texture_stack(1, matrix_identity_float4x4);
ClientArray vertex_array, texcoord_array, color_array;
std::unordered_map<GLuint, Texture> textures;
GLuint next_texture_name = 1;
GLint pack_alignment = 4;

// ---- Display lists
std::map<GLuint, std::vector<std::function<void()>>> lists;
GLuint next_list = 1;
GLuint compiling_list = 0;
bool compiling = false;
// Lists begun while another is being compiled (a font builds its glyph
// lists lazily, inside the list of the text that first uses it), and the
// texture binding to restore when the outermost list ends.
std::vector<GLuint> outer_lists;
GLuint binding_before_lists = 0;

// ---------------------------------------------------------------------------
// Helpers

simd_float4x4& current_matrix()
{
	switch (st.matrix_mode) {
		case GL_PROJECTION: return projection_stack.back();
		case GL_TEXTURE: return texture_stack.back();
		default: return modelview_stack.back();
	}
}

std::vector<simd_float4x4>& current_stack()
{
	switch (st.matrix_mode) {
		case GL_PROJECTION: return projection_stack;
		case GL_TEXTURE: return texture_stack;
		default: return modelview_stack;
	}
}

void mult_current(const simd_float4x4& m)
{
	simd_float4x4& c = current_matrix();
	c = simd_mul(c, m);
}

simd_float4x4 from_double(const GLdouble* m)
{
	return simd_matrix(simd_make_float4(m[0], m[1], m[2], m[3]), simd_make_float4(m[4], m[5], m[6], m[7]),
					   simd_make_float4(m[8], m[9], m[10], m[11]), simd_make_float4(m[12], m[13], m[14], m[15]));
}

simd_float4x4 rotation(double degrees, double x, double y, double z)
{
	const double len = std::sqrt(x * x + y * y + z * z);
	if (len == 0) return matrix_identity_float4x4;
	x /= len; y /= len; z /= len;
	const double a = degrees * M_PI / 180.0, c = std::cos(a), s = std::sin(a), t = 1 - c;
	return simd_matrix(
		simd_make_float4(x * x * t + c, y * x * t + z * s, x * z * t - y * s, 0),
		simd_make_float4(x * y * t - z * s, y * y * t + c, y * z * t + x * s, 0),
		simd_make_float4(x * z * t + y * s, y * z * t - x * s, z * z * t + c, 0),
		simd_make_float4(0, 0, 0, 1));
}

bool record(std::function<void()> f)
{
	if (!compiling) return false;
	lists[compiling_list].push_back(std::move(f));
	return true;
}

MTLBlendFactor blend_factor(GLenum f)
{
	switch (f) {
		case GL_ZERO: return MTLBlendFactorZero;
		case GL_ONE: return MTLBlendFactorOne;
		case GL_SRC_COLOR: return MTLBlendFactorSourceColor;
		case GL_ONE_MINUS_SRC_COLOR: return MTLBlendFactorOneMinusSourceColor;
		case GL_DST_COLOR: return MTLBlendFactorDestinationColor;
		case GL_ONE_MINUS_DST_COLOR: return MTLBlendFactorOneMinusDestinationColor;
		case GL_SRC_ALPHA: return MTLBlendFactorSourceAlpha;
		case GL_ONE_MINUS_SRC_ALPHA: return MTLBlendFactorOneMinusSourceAlpha;
		case GL_DST_ALPHA: return MTLBlendFactorDestinationAlpha;
		case GL_ONE_MINUS_DST_ALPHA: return MTLBlendFactorOneMinusDestinationAlpha;
		case GL_SRC_ALPHA_SATURATE: return MTLBlendFactorSourceAlphaSaturated;
		default: return MTLBlendFactorOne;
	}
}

MTLCompareFunction compare_function(GLenum f)
{
	switch (f) {
		case GL_NEVER: return MTLCompareFunctionNever;
		case GL_LESS: return MTLCompareFunctionLess;
		case GL_LEQUAL: return MTLCompareFunctionLessEqual;
		case GL_EQUAL: return MTLCompareFunctionEqual;
		case GL_GEQUAL: return MTLCompareFunctionGreaterEqual;
		case GL_GREATER: return MTLCompareFunctionGreater;
		case GL_NOTEQUAL: return MTLCompareFunctionNotEqual;
		default: return MTLCompareFunctionAlways;
	}
}

MTLStencilOperation stencil_operation(GLenum op)
{
	switch (op) {
		case GL_ZERO: return MTLStencilOperationZero;
		case GL_REPLACE: return MTLStencilOperationReplace;
		case GL_INCR: return MTLStencilOperationIncrementClamp;
		case GL_DECR: return MTLStencilOperationDecrementClamp;
		case GL_INVERT: return MTLStencilOperationInvert;
		default: return MTLStencilOperationKeep;
	}
}

int alpha_func_code(GLenum f)
{
	switch (f) {
		case GL_NEVER: return 1;
		case GL_LESS: return 2;
		case GL_LEQUAL: return 3;
		case GL_EQUAL: return 4;
		case GL_GEQUAL: return 5;
		case GL_GREATER: return 6;
		case GL_NOTEQUAL: return 7;
		default: return 0;
	}
}

bool init_metal()
{
	static bool attempted = false, ok = false;
	if (attempted) return ok;
	attempted = true;
	@autoreleasepool {
		device = MTLCreateSystemDefaultDevice();
		if (!device) return false;
		queue = [device newCommandQueue];
		NSError* error = nil;
		library = [device newLibraryWithSource:[NSString stringWithUTF8String:kDGLShaderSource] options:[MTLCompileOptions new] error:&error];
		if (!library) {
			logError("Durandal GL: shader compile failed: %s", error.localizedDescription.UTF8String);
			return false;
		}
		MTLRenderPipelineDescriptor* d = [MTLRenderPipelineDescriptor new];
		d.vertexFunction = [library newFunctionWithName:@"world_blit_vertex"];
		d.fragmentFunction = [library newFunctionWithName:@"world_blit_fragment"];
		d.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
		d.stencilAttachmentPixelFormat = MTLPixelFormatStencil8;
		world_blit_pipeline = [device newRenderPipelineStateWithDescriptor:d error:&error];
		if (!world_blit_pipeline) {
			logError("Durandal GL: world blit pipeline failed: %s", error.localizedDescription.UTF8String);
			return false;
		}

		for (int edr = 0; edr < 2; ++edr) {
			MTLRenderPipelineDescriptor* o = [MTLRenderPipelineDescriptor new];
			o.vertexFunction = [library newFunctionWithName:@"world_blit_vertex"];
			o.fragmentFunction = [library newFunctionWithName:@"output_fragment"];
			o.colorAttachments[0].pixelFormat = edr ? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm;
			id<MTLRenderPipelineState> p = [device newRenderPipelineStateWithDescriptor:o error:&error];
			if (!p) {
				logError("Durandal GL: output pipeline failed: %s", error.localizedDescription.UTF8String);
				return false;
			}
			(edr ? output_pipeline_edr : output_pipeline_sdr) = p;
		}

		MTLTextureDescriptor* wd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
		white_texture = [device newTextureWithDescriptor:wd];
		const uint32_t white = 0xFFFFFFFF;
		[white_texture replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&white bytesPerRow:4];
		default_sampler = [device newSamplerStateWithDescriptor:[MTLSamplerDescriptor new]];
		ok = true;
	}
	return ok;
}

id<MTLRenderPipelineState> pipeline_for(bool blend, GLenum src, GLenum dst, const bool mask[4], bool xor_op)
{
	const uint64_t key = (blend ? 1ull : 0) | (uint64_t(src & 0xFFFF) << 1) | (uint64_t(dst & 0xFFFF) << 17) |
		(uint64_t(mask[0]) << 33) | (uint64_t(mask[1]) << 34) | (uint64_t(mask[2]) << 35) | (uint64_t(mask[3]) << 36) |
		(uint64_t(xor_op) << 37);
	auto it = pipelines.find(key);
	if (it != pipelines.end()) return it->second;

	MTLRenderPipelineDescriptor* d = [MTLRenderPipelineDescriptor new];
	d.vertexFunction = [library newFunctionWithName:@"gl_vertex"];
	d.fragmentFunction = [library newFunctionWithName:xor_op ? @"gl_xor_fragment" : @"gl_fragment"];
	MTLRenderPipelineColorAttachmentDescriptor* a = d.colorAttachments[0];
	a.pixelFormat = MTLPixelFormatBGRA8Unorm;
	a.writeMask = (mask[0] ? MTLColorWriteMaskRed : 0) | (mask[1] ? MTLColorWriteMaskGreen : 0) |
		(mask[2] ? MTLColorWriteMaskBlue : 0) | (mask[3] ? MTLColorWriteMaskAlpha : 0);
	if (blend && !xor_op) {
		a.blendingEnabled = YES;
		a.sourceRGBBlendFactor = a.sourceAlphaBlendFactor = blend_factor(src);
		a.destinationRGBBlendFactor = a.destinationAlphaBlendFactor = blend_factor(dst);
	}
	d.stencilAttachmentPixelFormat = MTLPixelFormatStencil8;
	NSError* error = nil;
	id<MTLRenderPipelineState> p = [device newRenderPipelineStateWithDescriptor:d error:&error];
	if (!p)
		logError("Durandal GL: pipeline failed: %s", error.localizedDescription.UTF8String);
	pipelines[key] = p;
	return p;
}

id<MTLDepthStencilState> stencil_state_for(bool test, GLenum func, GLuint read_mask, GLuint write_mask, GLenum fail, GLenum zfail, GLenum zpass)
{
	auto op_code = [](GLenum op) -> uint64_t {
		switch (op) { case GL_ZERO: return 1; case GL_REPLACE: return 2; case GL_INCR: return 3;
			case GL_DECR: return 4; case GL_INVERT: return 5; default: return 0; }
	};
	const uint64_t key = test ? (1ull | (uint64_t(compare_function(func)) << 1) | (uint64_t(read_mask & 0xFF) << 5) |
		(uint64_t(write_mask & 0xFF) << 13) | (op_code(fail) << 21) | (op_code(zfail) << 24) | (op_code(zpass) << 27)) : 0;
	auto it = stencil_states.find(key);
	if (it != stencil_states.end()) return it->second;
	MTLDepthStencilDescriptor* d = [MTLDepthStencilDescriptor new];
	if (test) {
		MTLStencilDescriptor* s = [MTLStencilDescriptor new];
		s.stencilCompareFunction = compare_function(func);
		s.readMask = read_mask;
		s.writeMask = write_mask;
		s.stencilFailureOperation = stencil_operation(fail);
		s.depthFailureOperation = stencil_operation(zfail);
		s.depthStencilPassOperation = stencil_operation(zpass);
		d.frontFaceStencil = s;
		d.backFaceStencil = s;
	}
	id<MTLDepthStencilState> state = [device newDepthStencilStateWithDescriptor:d];
	stencil_states[key] = state;
	return state;
}

MTLSamplerAddressMode address_mode(GLenum w)
{
	switch (w) {
		case GL_CLAMP:
		case GL_CLAMP_TO_EDGE: return MTLSamplerAddressModeClampToEdge;
		case GL_MIRRORED_REPEAT: return MTLSamplerAddressModeMirrorRepeat;
		default: return MTLSamplerAddressModeRepeat;
	}
}

id<MTLSamplerState> sampler_for(const Texture& t)
{
	const uint64_t key = uint64_t(t.min_filter & 0xFFFF) | (uint64_t(t.mag_filter & 0xFFFF) << 16) |
		(uint64_t(t.wrap_s & 0xFFFF) << 32) | (uint64_t(t.wrap_t & 0xFFF) << 48) |
		(uint64_t(std::min(15, int(t.anisotropy))) << 60);
	auto it = samplers.find(key);
	if (it != samplers.end()) return it->second;
	MTLSamplerDescriptor* d = [MTLSamplerDescriptor new];
	d.magFilter = (t.mag_filter == GL_LINEAR) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
	const bool mipped = t.texture && t.texture.mipmapLevelCount > 1;
	switch (t.min_filter) {
		case GL_LINEAR: d.minFilter = MTLSamplerMinMagFilterLinear; break;
		case GL_NEAREST_MIPMAP_NEAREST: d.minFilter = MTLSamplerMinMagFilterNearest; if (mipped) d.mipFilter = MTLSamplerMipFilterNearest; break;
		case GL_LINEAR_MIPMAP_NEAREST: d.minFilter = MTLSamplerMinMagFilterLinear; if (mipped) d.mipFilter = MTLSamplerMipFilterNearest; break;
		case GL_NEAREST_MIPMAP_LINEAR: d.minFilter = MTLSamplerMinMagFilterNearest; if (mipped) d.mipFilter = MTLSamplerMipFilterLinear; break;
		case GL_LINEAR_MIPMAP_LINEAR: d.minFilter = MTLSamplerMinMagFilterLinear; if (mipped) d.mipFilter = MTLSamplerMipFilterLinear; break;
		default: d.minFilter = MTLSamplerMinMagFilterNearest; break;
	}
	d.sAddressMode = address_mode(t.wrap_s);
	d.tAddressMode = address_mode(t.wrap_t);
	d.maxAnisotropy = std::max(1, std::min(16, int(t.anisotropy)));
	id<MTLSamplerState> s = [device newSamplerStateWithDescriptor:d];
	samplers[key] = s;
	return s;
}

// ---- Frame management

void recycle_frame_buffers(std::vector<id<MTLBuffer>> used)
{
	std::lock_guard<std::mutex> lock(arena_mutex);
	for (auto& b : used)
		free_buffers.push_back(b);
}

// Glow (E1): this frame's glow image and where the world sits in the canvas;
// glow reaches at most this many times SDR white (or the display's headroom)
const float kMaximumGlow = 4.0f;
const float kBloomStrength = 0.35f;
id<MTLTexture> frame_glow;
id<MTLTexture> frame_bloom;
float world_distortion = 0, world_time = 0;	// underwater (W1)
bool frame_heat = false;		// heat shimmer this frame (SetHeatShimmer)
float frame_heat_time = 0;
// Development output capture (RequestOutputCapture)
bool output_capture_requested = false, output_capture_wait = false, output_capture_ready = false, output_capture_half = false;
id<MTLBuffer> output_capture_buffer;
simd_int2 output_capture_size;
MTLViewport frame_glow_viewport;

void begin_frame()
{
	if (command_buffer) return;
	int w = 0, h = 0;
	SDL_Metal_GetDrawableSize(window, &w, &h);
	if (w <= 0 || h <= 0) { w = 1; h = 1; }
	if (layer.drawableSize.width != w || layer.drawableSize.height != h)
		layer.drawableSize = CGSizeMake(w, h);
	if (!stencil_texture || target_width != w || target_height != h) {
		MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatStencil8 width:w height:h mipmapped:NO];
		d.usage = MTLTextureUsageRenderTarget;
		d.storageMode = MTLStorageModePrivate;
		stencil_texture = [device newTextureWithDescriptor:d];
		target_width = w;
		target_height = h;
	}
	if (!canvas || canvas.width != NSUInteger(w) || canvas.height != NSUInteger(h)) {
		MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:w height:h mipmapped:NO];
		d.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
		d.storageMode = MTLStorageModePrivate;
		canvas = [device newTextureWithDescriptor:d];
	}
	// Never more than kMaxFramesInFlight frames queued ahead of the GPU
	DurandalBenchmark::StageBegin(DurandalBenchmark::kStageWait);	// development timing
	dispatch_semaphore_wait(frames_in_flight, DISPATCH_TIME_FOREVER);
	DurandalBenchmark::StageEnd(DurandalBenchmark::kStageWait);
	command_buffer = [queue commandBuffer];
	target_cleared = false;
	frame_glow = nil;
	frame_bloom = nil;
}

id<MTLTexture> target_texture()
{
	return canvas;
}


// SDR: 8-bit layer, no colour matching (as the OpenGL window). EDR: half-float
// layer in extended linear Display P3, so SDR values keep the same look and
// values above 1.0 use the display's HDR headroom. Must happen before
// nextDrawable; takes effect on the next drawable.
void apply_output_mode()
{
	static const char* force = getenv("DURANDAL_HDR");	// development: 1/0 overrides the setting
	const bool want = force ? std::string(force) == "1" : Durandal::Enabled(Durandal::kHDROutput);
	if (want == edr_on && layer.pixelFormat == (want ? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm))
		return;
	edr_on = want;
	if (edr_on) {
		layer.pixelFormat = MTLPixelFormatRGBA16Float;
		layer.wantsExtendedDynamicRangeContent = YES;
		CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearDisplayP3);
		layer.colorspace = cs;
		CGColorSpaceRelease(cs);
	} else {
		layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
		layer.wantsExtendedDynamicRangeContent = NO;
		layer.colorspace = nil;
	}
	logNote("Durandal GL: HDR output %s", edr_on ? "on (EDR, half-float)" : "off");
}

// Takes the next drawable; blocks while the display holds them all, which
// is what paces frames to the refresh rate when vsync is on.
void acquire_drawable()
{
	if (drawable || offscreen) return;
	apply_output_mode();
	int w = 0, h = 0;
	SDL_Metal_GetDrawableSize(window, &w, &h);
	if (w > 0 && h > 0 && (layer.drawableSize.width != w || layer.drawableSize.height != h))
		layer.drawableSize = CGSizeMake(w, h);
	drawable = [layer nextDrawable];
}

void end_encoder()
{
	if (encoder) {
		[encoder endEncoding];
		encoder = nil;
	}
}

// Development (DURANDAL_GPU_TIMING): the canvas pass the world image opens
// is timed as the world's "blit and 2D" stage
bool time_next_canvas = false;

bool ensure_encoder()
{
	begin_frame();
	if (encoder) return true;
	if (!target_texture()) return false;
	MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
	pass.colorAttachments[0].texture = target_texture();
	pass.colorAttachments[0].loadAction = target_cleared ? MTLLoadActionLoad : MTLLoadActionClear;
	pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
	pass.colorAttachments[0].storeAction = MTLStoreActionStore;
	pass.stencilAttachment.texture = stencil_texture;
	pass.stencilAttachment.loadAction = target_cleared ? MTLLoadActionLoad : MTLLoadActionClear;
	pass.stencilAttachment.clearStencil = 0;
	pass.stencilAttachment.storeAction = MTLStoreActionStore;
	if (time_next_canvas) {
		time_next_canvas = false;
		DurandalMetal::TimeDisplayPass((__bridge void*)pass, DurandalMetal::kTimedCanvas);
	}
	encoder = [command_buffer renderCommandEncoderWithDescriptor:pass];
	target_cleared = true;
	return encoder != nil;
}

// Metal viewport and scissor from GL's bottom-left-origin boxes
void apply_viewport_and_scissor(bool full_target_scissor = false)
{
	const double H = target_height;
	MTLViewport vp = { double(st.viewport[0]), H - st.viewport[1] - st.viewport[3],
					   double(st.viewport[2]), double(st.viewport[3]), 0, 1 };
	[encoder setViewport:vp];
	int x0 = 0, y0 = 0, x1 = target_width, y1 = target_height;
	if (st.scissor_test && !full_target_scissor) {
		x0 = st.scissor_box[0];
		x1 = st.scissor_box[0] + st.scissor_box[2];
		y0 = target_height - (st.scissor_box[1] + st.scissor_box[3]);
		y1 = target_height - st.scissor_box[1];
	}
	x0 = std::clamp(x0, 0, target_width); x1 = std::clamp(x1, 0, target_width);
	y0 = std::clamp(y0, 0, target_height); y1 = std::clamp(y1, 0, target_height);
	MTLScissorRect r = { NSUInteger(x0), NSUInteger(y0), NSUInteger(std::max(0, x1 - x0)), NSUInteger(std::max(0, y1 - y0)) };
	[encoder setScissorRect:r];
}

bool scissor_is_empty()
{
	if (!st.scissor_test) return false;
	return st.scissor_box[2] <= 0 || st.scissor_box[3] <= 0;
}

// Copies vertices into this frame's arena and returns buffer + offset
bool upload(const std::vector<GLVertex>& verts, __strong id<MTLBuffer>& buffer, size_t& offset)
{
	const size_t bytes = verts.size() * sizeof(GLVertex);
	if (bytes > kArenaSize) return false;
	if (!current_buffer || current_offset + bytes > kArenaSize) {
		{
			std::lock_guard<std::mutex> lock(arena_mutex);
			if (!free_buffers.empty()) {
				current_buffer = free_buffers.back();
				free_buffers.pop_back();
			} else {
				current_buffer = nil;
			}
		}
		if (!current_buffer)
			current_buffer = [device newBufferWithLength:kArenaSize options:MTLResourceStorageModeShared];
		frame_buffers.push_back(current_buffer);
		current_offset = 0;
	}
	std::memcpy(static_cast<uint8_t*>(current_buffer.contents) + current_offset, verts.data(), bytes);
	buffer = current_buffer;
	offset = current_offset;
	current_offset += (bytes + 255) & ~size_t(255);
	return true;
}

// ---- Drawing

struct Resolved {
	GLenum mode;
	std::vector<simd_float4> position;	// object space
	std::vector<simd_float2> texcoord;	// empty: none
	std::vector<simd_float4> color;		// empty: current colour
};

double read_component(const ClientArray& a, int index, int component)
{
	const int type_size = (a.type == GL_DOUBLE) ? 8 : (a.type == GL_FLOAT || a.type == GL_INT || a.type == GL_UNSIGNED_INT) ? 4 :
		(a.type == GL_SHORT || a.type == GL_UNSIGNED_SHORT) ? 2 : 1;
	const size_t stride = a.stride ? a.stride : size_t(a.size) * type_size;
	const uint8_t* p = static_cast<const uint8_t*>(a.pointer) + stride * index + type_size * component;
	switch (a.type) {
		case GL_DOUBLE: { double v; std::memcpy(&v, p, 8); return v; }
		case GL_FLOAT: { float v; std::memcpy(&v, p, 4); return v; }
		case GL_INT: { int32_t v; std::memcpy(&v, p, 4); return v; }
		case GL_UNSIGNED_INT: { uint32_t v; std::memcpy(&v, p, 4); return v; }
		case GL_SHORT: { int16_t v; std::memcpy(&v, p, 2); return v; }
		case GL_UNSIGNED_SHORT: { uint16_t v; std::memcpy(&v, p, 2); return v; }
		case GL_UNSIGNED_BYTE: return *p;
		case GL_BYTE: return *reinterpret_cast<const int8_t*>(p);
		default: return 0;
	}
}

double normalised(const ClientArray& a, double v)
{
	switch (a.type) {
		case GL_UNSIGNED_BYTE: return v / 255.0;
		case GL_UNSIGNED_SHORT: return v / 65535.0;
		case GL_UNSIGNED_INT: return v / 4294967295.0;
		default: return v;
	}
}

// Reads the enabled client arrays for the given vertex indices
Resolved resolve(GLenum mode, const std::vector<uint32_t>& indices)
{
	Resolved r;
	r.mode = mode;
	if (!vertex_array.enabled || !vertex_array.pointer) return r;
	r.position.reserve(indices.size());
	for (uint32_t i : indices) {
		simd_float4 p = {0, 0, 0, 1};
		for (int c = 0; c < vertex_array.size && c < 4; ++c)
			p[c] = read_component(vertex_array, i, c);
		r.position.push_back(p);
	}
	if (texcoord_array.enabled && texcoord_array.pointer) {
		for (uint32_t i : indices) {
			simd_float2 t = {0, 0};
			for (int c = 0; c < texcoord_array.size && c < 2; ++c)
				t[c] = read_component(texcoord_array, i, c);
			r.texcoord.push_back(t);
		}
	}
	if (color_array.enabled && color_array.pointer) {
		for (uint32_t i : indices) {
			simd_float4 c = {0, 0, 0, 1};
			for (int k = 0; k < color_array.size && k < 4; ++k)
				c[k] = normalised(color_array, read_component(color_array, i, k));
			r.color.push_back(c);
		}
	}
	return r;
}

// Draws resolved vertices with the current state (execute or replay)
void execute(const Resolved& r)
{
	const size_t n = r.position.size();
	if (n < 3 || scissor_is_empty() || st.viewport[2] <= 0 || st.viewport[3] <= 0) return;
	if (!ensure_encoder()) return;

	const simd_float4x4 mv = modelview_stack.back();
	const simd_float4x4 proj = projection_stack.back();
	const simd_float4x4 tm = texture_stack.back();

	// Transform
	std::vector<GLVertex> v(n);
	std::vector<simd_float2> window(n);
	for (size_t i = 0; i < n; ++i) {
		const simd_float4 eye = simd_mul(mv, r.position[i]);
		const simd_float4 clip = simd_mul(proj, eye);
		v[i].position = clip;
		v[i].color = r.color.empty() ? st.color : r.color[i];
		if (!r.texcoord.empty()) {
			const simd_float4 t = simd_mul(tm, simd_make_float4(r.texcoord[i].x, r.texcoord[i].y, 0, 1));
			v[i].texcoord = simd_make_float2(t.x, t.y);
		} else {
			v[i].texcoord = simd_make_float2(0, 0);
		}
		for (int k = 0; k < 6; ++k)
			v[i].clip[k] = st.clip_enabled[k] ? simd_dot(st.clip_equation[k], eye) : 1.0f;
		const float w = (clip.w != 0) ? clip.w : 1e-6f;
		window[i] = simd_make_float2((clip.x / w * 0.5f + 0.5f) * st.viewport[2] + st.viewport[0],
									 (clip.y / w * 0.5f + 0.5f) * st.viewport[3] + st.viewport[1]);
	}

	// Assemble triangles, culling in window space as OpenGL does
	std::vector<GLVertex> tris;
	tris.reserve(n * 3);
	auto emit = [&](size_t a, size_t b, size_t c) {
		if (st.cull_face) {
			const float area = (window[b].x - window[a].x) * (window[c].y - window[a].y) -
				(window[c].x - window[a].x) * (window[b].y - window[a].y);
			const bool ccw = area > 0;
			const bool front = (st.front_face == GL_CCW) ? ccw : !ccw;
			if (st.cull_mode == GL_FRONT_AND_BACK) return;
			if (st.cull_mode == GL_BACK && !front) return;
			if (st.cull_mode == GL_FRONT && front) return;
		}
		tris.push_back(v[a]); tris.push_back(v[b]); tris.push_back(v[c]);
	};
	switch (r.mode) {
		case GL_TRIANGLES:
			for (size_t i = 0; i + 2 < n; i += 3) emit(i, i + 1, i + 2);
			break;
		case GL_TRIANGLE_STRIP:
			for (size_t i = 0; i + 2 < n; ++i) {
				if (i & 1) emit(i + 1, i, i + 2); else emit(i, i + 1, i + 2);
			}
			break;
		case GL_QUADS:
			for (size_t i = 0; i + 3 < n; i += 4) { emit(i, i + 1, i + 2); emit(i, i + 2, i + 3); }
			break;
		case GL_POLYGON:
		case GL_TRIANGLE_FAN:
		default:
			for (size_t i = 1; i + 1 < n; ++i) emit(0, i, i + 1);
			break;
	}
	if (tris.empty()) return;

	id<MTLBuffer> buffer;
	size_t offset;
	if (!upload(tris, buffer, offset)) return;

	const bool xor_op = st.logic_op && st.logic_opcode == GL_XOR;
	id<MTLRenderPipelineState> p = pipeline_for(st.blend, st.blend_src, st.blend_dst, st.color_mask, xor_op);
	if (!p) return;
	[encoder setRenderPipelineState:p];
	[encoder setDepthStencilState:stencil_state_for(st.stencil_test, st.stencil_func, st.stencil_value_mask,
		st.stencil_write_mask, st.stencil_fail, st.stencil_zfail, st.stencil_zpass)];
	[encoder setStencilReferenceValue:st.stencil_ref & 0xFF];
	[encoder setCullMode:MTLCullModeNone];
	apply_viewport_and_scissor();

	FragUniforms fu = { 0, st.env_mode == GL_REPLACE ? 1 : 0, st.alpha_test ? alpha_func_code(st.alpha_func) : 0, st.alpha_ref };
	id<MTLTexture> tex = white_texture;
	id<MTLSamplerState> smp = default_sampler;
	if (st.texture_2d && !r.texcoord.empty()) {
		auto it = textures.find(st.bound_texture);
		if (it != textures.end() && it->second.texture) {
			tex = it->second.texture;
			smp = sampler_for(it->second);
			fu.textured = 1;
		}
	}
	[encoder setVertexBuffer:buffer offset:offset atIndex:0];
	[encoder setFragmentBytes:&fu length:sizeof(fu) atIndex:0];
	[encoder setFragmentTexture:tex atIndex:0];
	[encoder setFragmentSamplerState:smp atIndex:0];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:tris.size()];
}

void draw(GLenum mode, const std::vector<uint32_t>& indices)
{
	Resolved r = resolve(mode, indices);
	if (compiling) {
		lists[compiling_list].push_back([r] { execute(r); });
		return;
	}
	execute(r);
}

// ---- Textures

struct RGBA { std::vector<uint8_t> data; };

// Converts client pixels to RGBA8
bool to_rgba(GLsizei w, GLsizei h, GLenum format, GLenum type, const GLvoid* pixels, std::vector<uint8_t>& out)
{
	out.assign(size_t(w) * h * 4, 0);
	if (!pixels) return true;
	const uint8_t* src = static_cast<const uint8_t*>(pixels);
	const size_t n = size_t(w) * h;
	if (type == GL_UNSIGNED_BYTE && format == GL_RGBA) {
		std::memcpy(out.data(), src, n * 4);
	} else if (type == GL_UNSIGNED_BYTE && format == GL_RGB) {
		for (size_t i = 0; i < n; ++i) { out[i * 4] = src[i * 3]; out[i * 4 + 1] = src[i * 3 + 1]; out[i * 4 + 2] = src[i * 3 + 2]; out[i * 4 + 3] = 255; }
	} else if (type == GL_UNSIGNED_BYTE && format == GL_LUMINANCE_ALPHA) {
		for (size_t i = 0; i < n; ++i) { out[i * 4] = out[i * 4 + 1] = out[i * 4 + 2] = src[i * 2]; out[i * 4 + 3] = src[i * 2 + 1]; }
	} else if (type == GL_UNSIGNED_BYTE && format == GL_LUMINANCE) {
		for (size_t i = 0; i < n; ++i) { out[i * 4] = out[i * 4 + 1] = out[i * 4 + 2] = src[i]; out[i * 4 + 3] = 255; }
	} else if (type == GL_UNSIGNED_BYTE && format == GL_ALPHA) {
		for (size_t i = 0; i < n; ++i) { out[i * 4] = out[i * 4 + 1] = out[i * 4 + 2] = 255; out[i * 4 + 3] = src[i]; }
	} else if ((type == GL_UNSIGNED_INT_8_8_8_8_REV || type == GL_UNSIGNED_BYTE) && format == GL_BGRA) {
		for (size_t i = 0; i < n; ++i) { out[i * 4] = src[i * 4 + 2]; out[i * 4 + 1] = src[i * 4 + 1]; out[i * 4 + 2] = src[i * 4]; out[i * 4 + 3] = src[i * 4 + 3]; }
	} else {
		logWarning("Durandal GL: unsupported texture format 0x%x/0x%x", format, type);
		return false;
	}
	return true;
}

bool is_srgb_format(GLint internal)
{
	return internal == GL_SRGB || internal == GL_SRGB_ALPHA || internal == GL_SRGB8 || internal == GL_SRGB8_ALPHA8;
}

bool is_rgb_only_format(GLint internal)
{
	switch (internal) {
		case 3: case GL_RGB: case GL_R3_G3_B2: case GL_RGB4: case GL_RGB5: case GL_RGB8: case GL_RGB10: case GL_RGB12:
		case GL_RGB16: case GL_SRGB: case GL_SRGB8: case GL_LUMINANCE: case GL_LUMINANCE8:
			return true;
		default:
			return false;
	}
}

bool wants_mipmaps(const Texture& t)
{
	return t.generate_mipmap || (t.min_filter != GL_NEAREST && t.min_filter != GL_LINEAR);
}

void generate_mipmaps(id<MTLTexture> tex)
{
	if (!tex || tex.mipmapLevelCount <= 1) return;
	id<MTLCommandBuffer> cb = [queue commandBuffer];
	id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
	[blit generateMipmapsForTexture:tex];
	[blit endEncoding];
	[cb commit];
}

void tex_image(Texture& t, GLint level, GLint internal, GLsizei w, GLsizei h, const std::vector<uint8_t>& rgba, MTLPixelFormat format, size_t bytes_per_row)
{
	if (level == 0) {
		const bool mip = wants_mipmaps(t);
		if (!t.texture || t.width != w || t.height != h || t.texture.pixelFormat != format || (t.texture.mipmapLevelCount > 1) != mip) {
			MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:std::max(1, int(w)) height:std::max(1, int(h)) mipmapped:mip];
			d.usage = MTLTextureUsageShaderRead;
			d.storageMode = MTLStorageModeShared;
			t.texture = [device newTextureWithDescriptor:d];
			t.width = w;
			t.height = h;
		}
	}
	if (!t.texture || level >= int(t.texture.mipmapLevelCount)) return;
	const int lw = std::max(1, t.width >> level), lh = std::max(1, t.height >> level);
	if (!rgba.empty())
		[t.texture replaceRegion:MTLRegionMake2D(0, 0, lw, lh) mipmapLevel:level withBytes:rgba.data() bytesPerRow:bytes_per_row];
	if (level == 0 && t.generate_mipmap)
		generate_mipmaps(t.texture);
}

}

// ---------------------------------------------------------------------------
// Display

namespace DurandalGL {

bool Wanted()
{
	if (!wanted_decided) {
		wanted_decided = true;
		if (const char* v = getenv("DURANDAL_METAL_DISPLAY"))
			wanted = std::string(v) == "1";
		else
			wanted = Durandal::Enabled(Durandal::kMetalRenderer);
	}
	return wanted;
}

bool Active()
{
	return active;
}

void Disable()
{
	wanted_decided = true;
	wanted = false;
}

bool CreateDisplay(SDL_Window* w, bool vsync)
{
	DestroyDisplay();
	if (!init_metal()) return false;
	window = w;
	metal_view = SDL_Metal_CreateView(window);
	if (!metal_view) {
		logError("Durandal GL: SDL_Metal_CreateView failed: %s", SDL_GetError());
		return false;
	}
	layer = (__bridge CAMetalLayer*)SDL_Metal_GetLayer(metal_view);
	layer.device = device;
	layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	layer.framebufferOnly = NO;		// screenshots and the parity check read it back
	// Three drawables (see drawable_count)
	vsync_on = vsync;
	layer.maximumDrawableCount = drawable_count(vsync);
	edr_on = false;
	edr_test = getenv("DURANDAL_EDR_TEST") && std::string(getenv("DURANDAL_EDR_TEST")) == "1";
	if (!frames_in_flight)
		frames_in_flight = dispatch_semaphore_create(kMaxFramesInFlight);
	offscreen = getenv("DURANDAL_BENCHMARK_OFFSCREEN") && std::string(getenv("DURANDAL_BENCHMARK_OFFSCREEN")) == "1";
	layer.displaySyncEnabled = vsync;
	int pw = 0, ph = 0;
	SDL_Metal_GetDrawableSize(window, &pw, &ph);
	layer.drawableSize = CGSizeMake(pw, ph);
	active = true;
	logNote("Durandal GL: Metal display %dx%d, vsync %s", pw, ph, vsync ? "on" : "off");
	return true;
}

void DestroyDisplay()
{
	if (!active) return;
	end_encoder();
	if (command_buffer) {
		[command_buffer commit];
		[command_buffer waitUntilCompleted];
		command_buffer = nil;
		dispatch_semaphore_signal(frames_in_flight);	// release the slot begin_frame took
	}
	drawable = nil;
	layer = nil;
	if (metal_view) {
		SDL_Metal_DestroyView(metal_view);
		metal_view = nullptr;
	}
	window = nullptr;
	active = false;
}

void SetVSync(bool vsync)
{
	vsync_on = vsync;
	if (layer) {
		layer.displaySyncEnabled = vsync;
		layer.maximumDrawableCount = drawable_count(vsync);
	}
}

void WaitForFrame()
{
	if (!active) return;
	@autoreleasepool {
		acquire_drawable();
	}
}

float EDRHeadroom()
{
	return window ? DurandalEDR_Headroom(window) : 1.0f;
}

void SetCollectPresentTimes(bool on)
{
	collect_present_times = on;
}

int DrawableCount()
{
	return layer ? int(layer.maximumDrawableCount) : 0;
}

float DisplayRefreshRate()
{
	return window ? DurandalEDR_RefreshRate(window) : 0.0f;
}

void TakePresentTimes(std::vector<double>& out)
{
	std::lock_guard<std::mutex> lock(present_mutex);
	out.swap(present_times);
	present_times.clear();
}

void DrawableSize(int& width, int& height)
{
	width = height = 0;
	if (window) SDL_Metal_GetDrawableSize(window, &width, &height);
}

void Present()
{
	if (!active) return;
	@autoreleasepool {
		ensure_encoder();	// a frame with no drawing still clears
		end_encoder();
		if (command_buffer) {
			acquire_drawable();	// usually already taken at the top of the frame (WaitForFrame)
			if (drawable) {
				MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
				pass.colorAttachments[0].texture = drawable.texture;
				pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
				pass.colorAttachments[0].storeAction = MTLStoreActionStore;
				DurandalMetal::TimeDisplayPass((__bridge void*)pass, DurandalMetal::kTimedOutput);
				id<MTLRenderCommandEncoder> out = [command_buffer renderCommandEncoderWithDescriptor:pass];
				const bool edr_target = drawable.texture.pixelFormat == MTLPixelFormatRGBA16Float;
				[out setRenderPipelineState:edr_target ? output_pipeline_edr : output_pipeline_sdr];
				const Durandal::Preferences& prefs = Durandal::Prefs();
				const bool graded = Durandal::Available() &&
					(prefs.scene_brightness != 0 || prefs.scene_contrast != 100 || prefs.scene_gamma != 100);
				struct { int decode; float test_headroom; simd_float2 size; int glow; float glow_gain; int bloom; float bloom_strength; simd_float4 glow_rect; simd_float4 grade; simd_float4 heat; } params = {
					edr_target ? 1 : 0,
					edr_test ? (edr_target ? std::max(1.0f, EDRHeadroom()) : 1.0f) : 0.0f,
					simd_make_float2(drawable.texture.width, drawable.texture.height),
					frame_glow ? 1 : 0,
					std::max(0.0f, std::min(EDRHeadroom(), kMaximumGlow) - 1.0f),
					frame_bloom ? 1 : 0,
					kBloomStrength,
					simd_make_float4(frame_glow_viewport.originX, frame_glow_viewport.originY,
									 std::max(1.0, frame_glow_viewport.width), std::max(1.0, frame_glow_viewport.height)),
					simd_make_float4(prefs.scene_brightness / 100.0f, prefs.scene_contrast / 100.0f,
									 std::max(prefs.scene_gamma, 1) / 100.0f, graded ? 1.0f : 0.0f),
					simd_make_float4(frame_heat ? 1.0f : 0.0f, frame_heat_time, 0, 0) };
				frame_heat = false;	// set again by the next world frame
				[out setFragmentBytes:&params length:sizeof(params) atIndex:0];
				[out setFragmentTexture:canvas atIndex:0];
				[out setFragmentTexture:(frame_glow ? frame_glow : canvas) atIndex:1];
				[out setFragmentTexture:(frame_bloom ? frame_bloom : canvas) atIndex:2];
				[out drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
				[out endEncoding];
				if (output_capture_requested) {
					output_capture_requested = false;
					id<MTLTexture> t = drawable.texture;
					const size_t bpp = edr_target ? 8 : 4;
					output_capture_buffer = [device newBufferWithLength:t.width * t.height * bpp options:MTLResourceStorageModeShared];
					id<MTLBlitCommandEncoder> blit = [command_buffer blitCommandEncoder];
					[blit copyFromTexture:t sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
							   sourceSize:MTLSizeMake(t.width, t.height, 1) toBuffer:output_capture_buffer destinationOffset:0
					  destinationBytesPerRow:t.width * bpp destinationBytesPerImage:t.width * t.height * bpp];
					[blit endEncoding];
					output_capture_size = simd_make_int2(int(t.width), int(t.height));
					output_capture_half = edr_target;
					output_capture_wait = true;
				}
				if (collect_present_times) {
					[drawable addPresentedHandler:^(id<MTLDrawable> d) {
						std::lock_guard<std::mutex> lock(present_mutex);
						present_times.push_back(d.presentedTime);
					}];
				}
				[command_buffer presentDrawable:drawable];
			}
			DurandalMetal::EndFrameTiming((__bridge void*)command_buffer);
			std::vector<id<MTLBuffer>> used;
			used.swap(frame_buffers);
			dispatch_semaphore_t in_flight = frames_in_flight;
			[command_buffer addCompletedHandler:^(id<MTLCommandBuffer>) {
				recycle_frame_buffers(used);
				dispatch_semaphore_signal(in_flight);
			}];
			[command_buffer commit];
			if (output_capture_wait) {
				output_capture_wait = false;
				[command_buffer waitUntilCompleted];
				output_capture_ready = true;
			}
		}
		command_buffer = nil;
		drawable = nil;
		current_buffer = nil;
		current_offset = 0;
	}
}

void RequestOutputCapture()
{
	output_capture_requested = true;
}

bool TakeOutputCapture(std::vector<uint8_t>& rgba, int& width, int& height)
{
	if (!output_capture_ready || !output_capture_buffer)
		return false;
	output_capture_ready = false;
	width = output_capture_size.x;
	height = output_capture_size.y;
	rgba.resize(size_t(width) * height * 4);
	if (output_capture_half) {
		// Linear half floats: clip to SDR and encode as sRGB
		const __fp16* p = static_cast<const __fp16*>(output_capture_buffer.contents);
		for (size_t i = 0; i < size_t(width) * height; ++i)
			for (int c = 0; c < 4; ++c) {
				float v = std::clamp(float(p[i * 4 + c]), 0.0f, 1.0f);
				if (c < 3) v = v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
				rgba[i * 4 + c] = uint8_t(std::lround(v * 255));
			}
	} else {
		const uint8_t* p = static_cast<const uint8_t*>(output_capture_buffer.contents);
		for (size_t i = 0; i < size_t(width) * height; ++i) {
			rgba[i * 4] = p[i * 4 + 2];
			rgba[i * 4 + 1] = p[i * 4 + 1];
			rgba[i * 4 + 2] = p[i * 4];
			rgba[i * 4 + 3] = 255;
		}
	}
	output_capture_buffer = nil;
	return true;
}

void* FrameCommandBuffer()
{
	if (!active) return nullptr;
	begin_frame();
	return (__bridge void*)command_buffer;
}

void EndScreenPass()
{
	end_encoder();
}

void DrawWorldImage(void* texture, float gamma, void* glow, void* bloom, void* ao, void* distance, float ao_far, bool ao_view,
					float distance_shade)
{
	time_next_canvas = true;
	if (!active || !ensure_encoder()) return;
	time_next_canvas = false;
	// Development: DURANDAL_GLOW_VIEW=1 shows the glow image as the world
	static const bool glow_view = getenv("DURANDAL_GLOW_VIEW") != nullptr;
	[encoder setRenderPipelineState:world_blit_pipeline];
	[encoder setDepthStencilState:stencil_state_for(false, GL_ALWAYS, 0xFF, 0xFF, GL_KEEP, GL_KEEP, GL_KEEP)];
	[encoder setCullMode:MTLCullModeNone];
	apply_viewport_and_scissor(true);
	id<MTLTexture> ao_texture = (__bridge id<MTLTexture>)ao;
	id<MTLTexture> distance_texture = (__bridge id<MTLTexture>)distance;
	const bool shadows = ao_texture && distance_texture && ao_far > 0;
	const bool shade = distance_texture && distance_shade > 0;
	struct { float gamma; int mark_world; float distortion; float time; float ao_far; float ao_view; simd_float2 ao_texel;
			 float shade; float pad[3]; } params = {
		(glow_view && glow) ? 1.0f : gamma, (glow || bloom) ? 1 : 0, world_distortion, world_time,
		shadows ? ao_far : 0.0f, ao_view ? 1.0f : 0.0f,
		shadows ? simd_make_float2(1.0f / ao_texture.width, 1.0f / ao_texture.height) : simd_make_float2(0, 0),
		shade ? distance_shade : 0.0f, { 0, 0, 0 } };
	[encoder setFragmentBytes:&params length:sizeof(params) atIndex:0];
	[encoder setFragmentTexture:(__bridge id<MTLTexture>)((glow_view && glow) ? glow : texture) atIndex:0];
	if (shadows)
		[encoder setFragmentTexture:ao_texture atIndex:1];
	if (shadows || shade)
		[encoder setFragmentTexture:distance_texture atIndex:2];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

	frame_glow = (__bridge id<MTLTexture>)glow;
	frame_bloom = (__bridge id<MTLTexture>)bloom;
	const double H = target_height;
	frame_glow_viewport = { double(st.viewport[0]), H - st.viewport[1] - st.viewport[3],
							double(st.viewport[2]), double(st.viewport[3]), 0, 1 };
}

void SetWorldDistortion(float amount, float time_seconds)
{
	world_distortion = amount;
	world_time = time_seconds;
}

void SetHeatShimmer(bool on, float time_seconds)
{
	frame_heat = on;
	frame_heat_time = time_seconds;
}

bool GlowWanted()
{
	static const bool glow_view = getenv("DURANDAL_GLOW_VIEW") != nullptr;
	return edr_on || glow_view;
}

void FlushAndWait()
{
	if (!active || !command_buffer) return;
	end_encoder();
	std::vector<id<MTLBuffer>> used;
	used.swap(frame_buffers);
	[command_buffer commit];
	[command_buffer waitUntilCompleted];
	recycle_frame_buffers(used);
	current_buffer = nil;
	current_offset = 0;
	// Continue the same frame (same drawable) in a new command buffer
	command_buffer = [queue commandBuffer];
}

}

namespace DurandalGL {
float PixelsPerUnit()
{
	if (compiling)
		return 0;
	const simd_float4x4 m = simd_mul(projection_stack.back(), modelview_stack.back());
	const float sx = std::fabs(m.columns[0][0]) * st.viewport[2] * 0.5f;
	const float sy = std::fabs(m.columns[1][1]) * st.viewport[3] * 0.5f;
	return std::sqrt(sx * sy);
}

void ReadFramebuffer(int width, int height, uint8_t* rgba)
{
	dgl_glPixelStorei(GL_PACK_ALIGNMENT, 1);
	dgl_glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	// The canvas's alpha marks the world for the glow; images are opaque
	for (size_t i = 3; i < size_t(width) * height * 4; i += 4)
		rgba[i] = 255;
	dgl_glPixelStorei(GL_PACK_ALIGNMENT, 4);
}
}

using DurandalGL::Active;

// ---------------------------------------------------------------------------
// The OpenGL subset. Each passes through to real OpenGL unless the Metal
// display is active.

#define PASS(call) do { if (!Active()) { call; return; } } while (0)
#define PASS_RET(call) do { if (!Active()) { return call; } } while (0)

static bool* enable_flag(GLenum cap)
{
	switch (cap) {
		case GL_BLEND: return &st.blend;
		case GL_ALPHA_TEST: return &st.alpha_test;
		case GL_TEXTURE_2D: return &st.texture_2d;
		case GL_SCISSOR_TEST: return &st.scissor_test;
		case GL_STENCIL_TEST: return &st.stencil_test;
		case GL_CULL_FACE: return &st.cull_face;
		case GL_DEPTH_TEST: return &st.depth_test;
		case GL_FOG: return &st.fog;
		case GL_COLOR_LOGIC_OP: return &st.logic_op;
		case GL_CLIP_PLANE0: case GL_CLIP_PLANE1: case GL_CLIP_PLANE2:
		case GL_CLIP_PLANE3: case GL_CLIP_PLANE4: case GL_CLIP_PLANE5:
			return &st.clip_enabled[cap - GL_CLIP_PLANE0];
		default: return nullptr;
	}
}

void dgl_glEnable(GLenum cap)
{
	PASS(glEnable(cap));
	if (record([cap] { dgl_glEnable(cap); })) return;
	if (bool* f = enable_flag(cap)) *f = true;
}

void dgl_glDisable(GLenum cap)
{
	PASS(glDisable(cap));
	if (record([cap] { dgl_glDisable(cap); })) return;
	if (bool* f = enable_flag(cap)) *f = false;
}

static ClientArray* client_array(GLenum array)
{
	switch (array) {
		case GL_VERTEX_ARRAY: return &vertex_array;
		case GL_TEXTURE_COORD_ARRAY: return &texcoord_array;
		case GL_COLOR_ARRAY: return &color_array;
		default: return nullptr;
	}
}

// Client-side state is never compiled into display lists (OpenGL rule)
void dgl_glEnableClientState(GLenum array) { PASS(glEnableClientState(array)); if (auto a = client_array(array)) a->enabled = true; }
void dgl_glDisableClientState(GLenum array) { PASS(glDisableClientState(array)); if (auto a = client_array(array)) a->enabled = false; }
void dgl_glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
	PASS(glVertexPointer(size, type, stride, pointer));
	vertex_array.size = size; vertex_array.type = type; vertex_array.stride = stride; vertex_array.pointer = pointer;
}
void dgl_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
	PASS(glTexCoordPointer(size, type, stride, pointer));
	texcoord_array.size = size; texcoord_array.type = type; texcoord_array.stride = stride; texcoord_array.pointer = pointer;
}
void dgl_glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
	PASS(glColorPointer(size, type, stride, pointer));
	color_array.size = size; color_array.type = type; color_array.stride = stride; color_array.pointer = pointer;
}

void dgl_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	PASS(glDrawArrays(mode, first, count));
	std::vector<uint32_t> idx(count > 0 ? count : 0);
	for (GLsizei i = 0; i < count; ++i) idx[i] = first + i;
	draw(mode, idx);
}

void dgl_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
{
	PASS(glDrawElements(mode, count, type, indices));
	std::vector<uint32_t> idx(count > 0 ? count : 0);
	for (GLsizei i = 0; i < count; ++i) {
		switch (type) {
			case GL_UNSIGNED_BYTE: idx[i] = static_cast<const uint8_t*>(indices)[i]; break;
			case GL_UNSIGNED_SHORT: idx[i] = static_cast<const uint16_t*>(indices)[i]; break;
			default: idx[i] = static_cast<const uint32_t*>(indices)[i]; break;
		}
	}
	draw(mode, idx);
}

// ---- Matrices

void dgl_glMatrixMode(GLenum mode) { PASS(glMatrixMode(mode)); if (record([mode] { dgl_glMatrixMode(mode); })) return; st.matrix_mode = mode; }
void dgl_glLoadIdentity(void) { PASS(glLoadIdentity()); if (record([] { dgl_glLoadIdentity(); })) return; current_matrix() = matrix_identity_float4x4; }
void dgl_glLoadMatrixd(const GLdouble* m)
{
	PASS(glLoadMatrixd(m));
	simd_float4x4 mm = from_double(m);
	if (record([mm] { current_matrix() = mm; })) return;
	current_matrix() = mm;
}
void dgl_glMultMatrixd(const GLdouble* m)
{
	PASS(glMultMatrixd(m));
	simd_float4x4 mm = from_double(m);
	if (record([mm] { mult_current(mm); })) return;
	mult_current(mm);
}
void dgl_glPushMatrix(void)
{
	PASS(glPushMatrix());
	if (record([] { dgl_glPushMatrix(); })) return;
	auto& s = current_stack();
	s.push_back(s.back());
}
void dgl_glPopMatrix(void)
{
	PASS(glPopMatrix());
	if (record([] { dgl_glPopMatrix(); })) return;
	auto& s = current_stack();
	if (s.size() > 1) s.pop_back();
}
void dgl_glTranslated(GLdouble x, GLdouble y, GLdouble z)
{
	PASS(glTranslated(x, y, z));
	if (record([x, y, z] { dgl_glTranslated(x, y, z); })) return;
	simd_float4x4 t = matrix_identity_float4x4;
	t.columns[3] = simd_make_float4(x, y, z, 1);
	mult_current(t);
}
void dgl_glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	PASS(glTranslatef(x, y, z));
	if (record([x, y, z] { dgl_glTranslatef(x, y, z); })) return;
	simd_float4x4 t = matrix_identity_float4x4;
	t.columns[3] = simd_make_float4(x, y, z, 1);
	mult_current(t);
}
void dgl_glScaled(GLdouble x, GLdouble y, GLdouble z)
{
	PASS(glScaled(x, y, z));
	if (record([x, y, z] { dgl_glScaled(x, y, z); })) return;
	mult_current(simd_matrix(simd_make_float4(x, 0, 0, 0), simd_make_float4(0, y, 0, 0), simd_make_float4(0, 0, z, 0), simd_make_float4(0, 0, 0, 1)));
}
void dgl_glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	PASS(glScalef(x, y, z));
	if (record([x, y, z] { dgl_glScalef(x, y, z); })) return;
	mult_current(simd_matrix(simd_make_float4(x, 0, 0, 0), simd_make_float4(0, y, 0, 0), simd_make_float4(0, 0, z, 0), simd_make_float4(0, 0, 0, 1)));
}
void dgl_glRotated(GLdouble a, GLdouble x, GLdouble y, GLdouble z)
{
	PASS(glRotated(a, x, y, z));
	if (record([a, x, y, z] { dgl_glRotated(a, x, y, z); })) return;
	mult_current(rotation(a, x, y, z));
}
void dgl_glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z)
{
	PASS(glRotatef(a, x, y, z));
	if (record([a, x, y, z] { dgl_glRotatef(a, x, y, z); })) return;
	mult_current(rotation(a, x, y, z));
}
void dgl_glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
	PASS(glOrtho(l, r, b, t, n, f));
	if (record([=] { dgl_glOrtho(l, r, b, t, n, f); })) return;
	mult_current(simd_matrix(
		simd_make_float4(2 / (r - l), 0, 0, 0),
		simd_make_float4(0, 2 / (t - b), 0, 0),
		simd_make_float4(0, 0, -2 / (f - n), 0),
		simd_make_float4(-(r + l) / (r - l), -(t + b) / (t - b), -(f + n) / (f - n), 1)));
}

// ---- Colour

static void set_color(float r, float g, float b, float a)
{
	simd_float4 c = simd_make_float4(r, g, b, a);
	if (record([c] { st.color = c; })) return;
	st.color = c;
}
void dgl_glColor3f(GLfloat r, GLfloat g, GLfloat b) { PASS(glColor3f(r, g, b)); set_color(r, g, b, 1); }
void dgl_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { PASS(glColor4f(r, g, b, a)); set_color(r, g, b, a); }
void dgl_glColor3fv(const GLfloat* v) { PASS(glColor3fv(v)); set_color(v[0], v[1], v[2], 1); }
void dgl_glColor4fv(const GLfloat* v) { PASS(glColor4fv(v)); set_color(v[0], v[1], v[2], v[3]); }
void dgl_glColor3ub(GLubyte r, GLubyte g, GLubyte b) { PASS(glColor3ub(r, g, b)); set_color(r / 255.f, g / 255.f, b / 255.f, 1); }
void dgl_glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) { PASS(glColor4ub(r, g, b, a)); set_color(r / 255.f, g / 255.f, b / 255.f, a / 255.f); }
void dgl_glColor3us(GLushort r, GLushort g, GLushort b) { PASS(glColor3us(r, g, b)); set_color(r / 65535.f, g / 65535.f, b / 65535.f, 1); }
void dgl_glColor4us(GLushort r, GLushort g, GLushort b, GLushort a) { PASS(glColor4us(r, g, b, a)); set_color(r / 65535.f, g / 65535.f, b / 65535.f, a / 65535.f); }
void dgl_glColor3usv(const GLushort* v) { PASS(glColor3usv(v)); set_color(v[0] / 65535.f, v[1] / 65535.f, v[2] / 65535.f, 1); }
void dgl_glColor4usv(const GLushort* v) { PASS(glColor4usv(v)); set_color(v[0] / 65535.f, v[1] / 65535.f, v[2] / 65535.f, v[3] / 65535.f); }

// ---- Fixed-function state

void dgl_glBlendFunc(GLenum s, GLenum d) { PASS(glBlendFunc(s, d)); if (record([s, d] { dgl_glBlendFunc(s, d); })) return; st.blend_src = s; st.blend_dst = d; }
void dgl_glAlphaFunc(GLenum f, GLclampf ref) { PASS(glAlphaFunc(f, ref)); if (record([f, ref] { dgl_glAlphaFunc(f, ref); })) return; st.alpha_func = f; st.alpha_ref = ref; }
void dgl_glLogicOp(GLenum op) { PASS(glLogicOp(op)); if (record([op] { dgl_glLogicOp(op); })) return; st.logic_opcode = op; }
void dgl_glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
	PASS(glColorMask(r, g, b, a));
	if (record([=] { dgl_glColorMask(r, g, b, a); })) return;
	st.color_mask[0] = r; st.color_mask[1] = g; st.color_mask[2] = b; st.color_mask[3] = a;
}
void dgl_glCullFace(GLenum m) { PASS(glCullFace(m)); if (record([m] { dgl_glCullFace(m); })) return; st.cull_mode = m; }
void dgl_glFrontFace(GLenum m) { PASS(glFrontFace(m)); if (record([m] { dgl_glFrontFace(m); })) return; st.front_face = m; }
void dgl_glDepthFunc(GLenum f) { PASS(glDepthFunc(f)); }
void dgl_glDepthRange(GLclampd n, GLclampd f) { PASS(glDepthRange(n, f)); }
void dgl_glFogf(GLenum p, GLfloat v) { PASS(glFogf(p, v)); }
void dgl_glFogfv(GLenum p, const GLfloat* v) { PASS(glFogfv(p, v)); }
void dgl_glPolygonStipple(const GLubyte* m) { PASS(glPolygonStipple(m)); }

void dgl_glClipPlane(GLenum plane, const GLdouble* e)
{
	PASS(glClipPlane(plane, e));
	const int i = plane - GL_CLIP_PLANE0;
	if (i < 0 || i >= 6) return;
	simd_float4 eq = simd_make_float4(e[0], e[1], e[2], e[3]);
	auto apply = [i, eq] {
		// OpenGL stores the plane in eye space: p_eye = p * inverse(modelview)
		st.clip_equation[i] = simd_mul(eq, simd_inverse(modelview_stack.back()));
	};
	if (record(apply)) return;
	apply();
}

void dgl_glStencilFunc(GLenum f, GLint ref, GLuint mask)
{
	PASS(glStencilFunc(f, ref, mask));
	if (record([=] { dgl_glStencilFunc(f, ref, mask); })) return;
	st.stencil_func = f; st.stencil_ref = ref; st.stencil_value_mask = mask;
}
void dgl_glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	PASS(glStencilOp(fail, zfail, zpass));
	if (record([=] { dgl_glStencilOp(fail, zfail, zpass); })) return;
	st.stencil_fail = fail; st.stencil_zfail = zfail; st.stencil_zpass = zpass;
}
void dgl_glStencilMask(GLuint mask) { PASS(glStencilMask(mask)); if (record([mask] { dgl_glStencilMask(mask); })) return; st.stencil_write_mask = mask; }
void dgl_glClearStencil(GLint s) { PASS(glClearStencil(s)); st.clear_stencil = s; }
void dgl_glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) { PASS(glClearColor(r, g, b, a)); st.clear_color = simd_make_float4(r, g, b, a); }

void dgl_glClear(GLbitfield mask)
{
	PASS(glClear(mask));
	if (record([mask] { dgl_glClear(mask); })) return;
	if (!(mask & (GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) || scissor_is_empty()) return;
	if (!ensure_encoder()) return;

	// A full-target quad, limited only by the scissor box (as glClear)
	GLVertex q[6];
	const simd_float2 corners[6] = { {-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1} };
	for (int i = 0; i < 6; ++i) {
		q[i] = GLVertex{};
		q[i].position = simd_make_float4(corners[i].x, corners[i].y, 0, 1);
		q[i].color = st.clear_color;
		for (int k = 0; k < 6; ++k) q[i].clip[k] = 1;
	}
	const bool clear_color = mask & GL_COLOR_BUFFER_BIT;
	const bool clear_stencil = mask & GL_STENCIL_BUFFER_BIT;
	const bool no_color[4] = {false, false, false, false};
	id<MTLRenderPipelineState> p = pipeline_for(false, GL_ONE, GL_ZERO, clear_color ? st.color_mask : no_color, false);
	if (!p) return;
	[encoder setRenderPipelineState:p];
	[encoder setDepthStencilState:clear_stencil ?
		stencil_state_for(true, GL_ALWAYS, 0xFF, st.stencil_write_mask, GL_REPLACE, GL_REPLACE, GL_REPLACE) :
		stencil_state_for(false, GL_ALWAYS, 0xFF, 0xFF, GL_KEEP, GL_KEEP, GL_KEEP)];
	[encoder setStencilReferenceValue:st.clear_stencil & 0xFF];
	[encoder setCullMode:MTLCullModeNone];
	// full target viewport; scissor still applies
	[encoder setViewport:(MTLViewport){0, 0, double(target_width), double(target_height), 0, 1}];
	int x0 = 0, y0 = 0, x1 = target_width, y1 = target_height;
	if (st.scissor_test) {
		x0 = std::clamp(st.scissor_box[0], 0, target_width);
		x1 = std::clamp(st.scissor_box[0] + st.scissor_box[2], 0, target_width);
		y0 = std::clamp(target_height - (st.scissor_box[1] + st.scissor_box[3]), 0, target_height);
		y1 = std::clamp(target_height - st.scissor_box[1], 0, target_height);
	}
	if (x1 <= x0 || y1 <= y0) return;
	[encoder setScissorRect:(MTLScissorRect){NSUInteger(x0), NSUInteger(y0), NSUInteger(x1 - x0), NSUInteger(y1 - y0)}];
	FragUniforms fu = {0, 0, 0, 0};
	[encoder setVertexBytes:q length:sizeof(q) atIndex:0];
	[encoder setFragmentBytes:&fu length:sizeof(fu) atIndex:0];
	[encoder setFragmentTexture:white_texture atIndex:0];
	[encoder setFragmentSamplerState:default_sampler atIndex:0];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
}

void dgl_glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
	PASS(glViewport(x, y, w, h));
	if (record([=] { dgl_glViewport(x, y, w, h); })) return;
	st.viewport[0] = x; st.viewport[1] = y; st.viewport[2] = w; st.viewport[3] = h;
}
void dgl_glScissor(GLint x, GLint y, GLsizei w, GLsizei h)
{
	PASS(glScissor(x, y, w, h));
	if (record([=] { dgl_glScissor(x, y, w, h); })) return;
	st.scissor_box[0] = x; st.scissor_box[1] = y; st.scissor_box[2] = w; st.scissor_box[3] = h;
}

void dgl_glPushAttrib(GLbitfield mask)
{
	PASS(glPushAttrib(mask));
	if (record([mask] { dgl_glPushAttrib(mask); })) return;
	attrib_stack.push_back({mask, st});
}

void dgl_glPopAttrib(void)
{
	PASS(glPopAttrib());
	if (record([] { dgl_glPopAttrib(); })) return;
	if (attrib_stack.empty()) return;
	const AttribFrame f = attrib_stack.back();
	attrib_stack.pop_back();
	const State& s = f.state;
	const GLbitfield m = f.mask;
	if (m & GL_CURRENT_BIT) st.color = s.color;
	if (m & GL_ENABLE_BIT) {
		st.blend = s.blend; st.alpha_test = s.alpha_test; st.texture_2d = s.texture_2d;
		st.scissor_test = s.scissor_test; st.stencil_test = s.stencil_test; st.cull_face = s.cull_face;
		st.depth_test = s.depth_test; st.fog = s.fog; st.logic_op = s.logic_op;
		std::copy(s.clip_enabled, s.clip_enabled + 6, st.clip_enabled);
	}
	if (m & GL_COLOR_BUFFER_BIT) {
		st.blend = s.blend; st.alpha_test = s.alpha_test; st.logic_op = s.logic_op;
		st.blend_src = s.blend_src; st.blend_dst = s.blend_dst;
		st.alpha_func = s.alpha_func; st.alpha_ref = s.alpha_ref; st.logic_opcode = s.logic_opcode;
		std::copy(s.color_mask, s.color_mask + 4, st.color_mask);
		st.clear_color = s.clear_color;
	}
	if (m & GL_DEPTH_BUFFER_BIT) st.depth_test = s.depth_test;
	if (m & GL_FOG_BIT) st.fog = s.fog;
	if (m & GL_POLYGON_BIT) { st.cull_face = s.cull_face; st.cull_mode = s.cull_mode; st.front_face = s.front_face; }
	if (m & GL_SCISSOR_BIT) { st.scissor_test = s.scissor_test; std::copy(s.scissor_box, s.scissor_box + 4, st.scissor_box); }
	if (m & GL_VIEWPORT_BIT) std::copy(s.viewport, s.viewport + 4, st.viewport);
	if (m & GL_STENCIL_BUFFER_BIT) {
		st.stencil_test = s.stencil_test; st.stencil_func = s.stencil_func; st.stencil_ref = s.stencil_ref;
		st.stencil_value_mask = s.stencil_value_mask; st.stencil_write_mask = s.stencil_write_mask;
		st.stencil_fail = s.stencil_fail; st.stencil_zfail = s.stencil_zfail; st.stencil_zpass = s.stencil_zpass;
		st.clear_stencil = s.clear_stencil;
	}
	if (m & GL_TEXTURE_BIT) { st.bound_texture = s.bound_texture; st.env_mode = s.env_mode; st.texture_2d = s.texture_2d; }
	if (m & GL_TRANSFORM_BIT) {
		st.matrix_mode = s.matrix_mode;
		std::copy(s.clip_enabled, s.clip_enabled + 6, st.clip_enabled);
		std::copy(s.clip_equation, s.clip_equation + 6, st.clip_equation);
	}
}

// ---- Textures

void dgl_glGenTextures(GLsizei n, GLuint* names)
{
	PASS(glGenTextures(n, names));
	for (GLsizei i = 0; i < n; ++i) {
		names[i] = next_texture_name++;
		textures[names[i]] = Texture{};
	}
}

void dgl_glDeleteTextures(GLsizei n, const GLuint* names)
{
	PASS(glDeleteTextures(n, names));
	for (GLsizei i = 0; i < n; ++i) {
		textures.erase(names[i]);
		if (st.bound_texture == names[i]) st.bound_texture = 0;
	}
}

void dgl_glBindTexture(GLenum target, GLuint name)
{
	PASS(glBindTexture(target, name));
	// While compiling, also bind now: the texture being built (e.g. a
	// font's, created inside a list) must receive the uploads that follow.
	// glEndList restores the binding.
	record([target, name] { dgl_glBindTexture(target, name); });
	if (target != GL_TEXTURE_2D) return;
	st.bound_texture = name;
	if (name && !textures.count(name)) textures[name] = Texture{};
}

static Texture* bound()
{
	auto it = textures.find(st.bound_texture);
	return it == textures.end() ? nullptr : &it->second;
}

void dgl_glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	PASS(glTexParameteri(target, pname, param));
	Texture* t = bound();
	if (!t) return;
	switch (pname) {
		case GL_TEXTURE_MIN_FILTER: t->min_filter = param; break;
		case GL_TEXTURE_MAG_FILTER: t->mag_filter = param; break;
		case GL_TEXTURE_WRAP_S: t->wrap_s = param; break;
		case GL_TEXTURE_WRAP_T: t->wrap_t = param; break;
		case 0x8191: t->generate_mipmap = param != 0; break;	// GL_GENERATE_MIPMAP(_SGIS)
		default: break;
	}
}

void dgl_glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	PASS(glTexParameterf(target, pname, param));
	Texture* t = bound();
	if (!t) return;
	if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT) t->anisotropy = param;
	else dgl_glTexParameteri(target, pname, GLint(param));
}

void dgl_glTexEnvi(GLenum target, GLenum pname, GLint param)
{
	PASS(glTexEnvi(target, pname, param));
	if (record([=] { dgl_glTexEnvi(target, pname, param); })) return;
	if (pname == GL_TEXTURE_ENV_MODE) st.env_mode = param;
}

void dgl_glTexImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const GLvoid* pixels)
{
	PASS(glTexImage2D(target, level, internal, w, h, border, format, type, pixels));
	Texture* t = bound();
	if (!t || target != GL_TEXTURE_2D) return;
	std::vector<uint8_t> rgba;
	if (!to_rgba(w, h, format, type, pixels, rgba)) return;
	if (is_rgb_only_format(internal))
		for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
	t->srgb = is_srgb_format(internal);
	tex_image(*t, level, internal, w, h, pixels ? rgba : std::vector<uint8_t>(), t->srgb ? MTLPixelFormatRGBA8Unorm_sRGB : MTLPixelFormatRGBA8Unorm, size_t(std::max(1, int(w) >> 0)) * 4);
}

void dgl_glCompressedTexImage2DARB(GLenum target, GLint level, GLenum internal, GLsizei w, GLsizei h, GLint border, GLsizei size, const GLvoid* data)
{
	PASS(glCompressedTexImage2DARB(target, level, internal, w, h, border, size, data));
	Texture* t = bound();
	if (!t || target != GL_TEXTURE_2D || !data) return;
	MTLPixelFormat f;
	size_t block_bytes;
	switch (internal) {
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT: case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: f = MTLPixelFormatBC1_RGBA; block_bytes = 8; break;
		case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT: case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT: f = MTLPixelFormatBC1_RGBA_sRGB; block_bytes = 8; break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT: f = MTLPixelFormatBC2_RGBA; block_bytes = 16; break;
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT: f = MTLPixelFormatBC2_RGBA_sRGB; block_bytes = 16; break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: f = MTLPixelFormatBC3_RGBA; block_bytes = 16; break;
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT: f = MTLPixelFormatBC3_RGBA_sRGB; block_bytes = 16; break;
		default: logWarning("Durandal GL: unsupported compressed format 0x%x", internal); return;
	}
	if (level == 0 && (!t->texture || t->texture.pixelFormat != f || t->width != w || t->height != h)) {
		MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f width:w height:h mipmapped:wants_mipmaps(*t)];
		d.usage = MTLTextureUsageShaderRead;
		d.storageMode = MTLStorageModeShared;
		t->texture = [device newTextureWithDescriptor:d];
		t->width = w; t->height = h;
	}
	if (!t->texture || level >= int(t->texture.mipmapLevelCount)) return;
	const int lw = std::max(1, t->width >> level), lh = std::max(1, t->height >> level);
	const size_t row = size_t((lw + 3) / 4) * block_bytes;
	[t->texture replaceRegion:MTLRegionMake2D(0, 0, lw, lh) mipmapLevel:level withBytes:data bytesPerRow:row];
}

GLint dgl_gluBuild2DMipmaps(GLenum target, GLint internal, GLsizei w, GLsizei h, GLenum format, GLenum type, const void* data)
{
	PASS_RET(gluBuild2DMipmaps(target, internal, w, h, format, type, data));
	Texture* t = bound();
	if (!t) return 0;
	const bool saved = t->generate_mipmap;
	t->generate_mipmap = true;
	dgl_glTexImage2D(target, 0, internal, w, h, 0, format, type, data);
	t->generate_mipmap = saved;
	return 0;
}

GLint dgl_gluScaleImage(GLenum format, GLsizei wIn, GLsizei hIn, GLenum typeIn, const void* dataIn,
						GLsizei wOut, GLsizei hOut, GLenum typeOut, GLvoid* dataOut)
{
	PASS_RET(gluScaleImage(format, wIn, hIn, typeIn, dataIn, wOut, hOut, typeOut, dataOut));
	if (format != GL_RGBA || typeIn != GL_UNSIGNED_BYTE || typeOut != GL_UNSIGNED_BYTE) return GLU_INVALID_ENUM;
	// Box filter, as good as needed for shrinking over-size textures
	const uint8_t* in = static_cast<const uint8_t*>(dataIn);
	uint8_t* out = static_cast<uint8_t*>(dataOut);
	for (int y = 0; y < hOut; ++y) {
		const int y0 = y * hIn / hOut, y1 = std::max(y0 + 1, (y + 1) * hIn / hOut);
		for (int x = 0; x < wOut; ++x) {
			const int x0 = x * wIn / wOut, x1 = std::max(x0 + 1, (x + 1) * wIn / wOut);
			unsigned sum[4] = {0, 0, 0, 0}, count = 0;
			for (int yy = y0; yy < y1; ++yy)
				for (int xx = x0; xx < x1; ++xx, ++count)
					for (int c = 0; c < 4; ++c) sum[c] += in[(size_t(yy) * wIn + xx) * 4 + c];
			for (int c = 0; c < 4; ++c) out[(size_t(y) * wOut + x) * 4 + c] = uint8_t(sum[c] / count);
		}
	}
	return 0;
}

void dgl_glPixelStorei(GLenum pname, GLint param)
{
	PASS(glPixelStorei(pname, param));
	if (pname == GL_PACK_ALIGNMENT) pack_alignment = param;
}

void dgl_glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, GLvoid* pixels)
{
	PASS(glReadPixels(x, y, w, h, format, type, pixels));
	if (!pixels || w <= 0 || h <= 0 || !ensure_encoder()) return;
	const int ty = target_height - (y + h);
	if (x < 0 || ty < 0 || x + w > target_width || ty + h > target_height) return;
	end_encoder();
	id<MTLBuffer> buf = [device newBufferWithLength:size_t(w) * h * 4 options:MTLResourceStorageModeShared];
	id<MTLBlitCommandEncoder> blit = [command_buffer blitCommandEncoder];
	[blit copyFromTexture:target_texture() sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(x, ty, 0)
			   sourceSize:MTLSizeMake(w, h, 1) toBuffer:buf destinationOffset:0 destinationBytesPerRow:size_t(w) * 4
	  destinationBytesPerImage:size_t(w) * h * 4];
	[blit endEncoding];
	DurandalGL::FlushAndWait();
	const uint8_t* src = static_cast<const uint8_t*>(buf.contents);	// BGRA, top-down
	uint8_t* dst = static_cast<uint8_t*>(pixels);
	const int bpp = (format == GL_RGB) ? 3 : 4;
	const size_t row_bytes = size_t(w) * bpp;
	const size_t stride = (row_bytes + pack_alignment - 1) / pack_alignment * pack_alignment;
	for (int row = 0; row < h; ++row) {
		const uint8_t* s = src + size_t(h - 1 - row) * w * 4;	// OpenGL rows run bottom-up
		uint8_t* d = dst + stride * row;
		for (int i = 0; i < w; ++i) {
			const uint8_t b = s[i * 4], g = s[i * 4 + 1], r = s[i * 4 + 2], a = s[i * 4 + 3];
			if (format == GL_BGRA) { d[i * 4] = b; d[i * 4 + 1] = g; d[i * 4 + 2] = r; d[i * 4 + 3] = a; }
			else if (bpp == 3) { d[i * 3] = r; d[i * 3 + 1] = g; d[i * 3 + 2] = b; }
			else { d[i * 4] = r; d[i * 4 + 1] = g; d[i * 4 + 2] = b; d[i * 4 + 3] = a; }
		}
	}
}

// ---- Display lists

GLuint dgl_glGenLists(GLsizei range)
{
	PASS_RET(glGenLists(range));
	const GLuint first = next_list;
	next_list += range;
	return first;
}

void dgl_glDeleteLists(GLuint list, GLsizei range)
{
	PASS(glDeleteLists(list, range));
	for (GLsizei i = 0; i < range; ++i) lists.erase(list + i);
}

void dgl_glNewList(GLuint list, GLenum mode)
{
	PASS(glNewList(list, mode));
	if (compiling)
		outer_lists.push_back(compiling_list);
	else
		binding_before_lists = st.bound_texture;
	lists[list].clear();
	compiling_list = list;
	compiling = true;
}

void dgl_glEndList(void)
{
	PASS(glEndList());
	if (!outer_lists.empty()) {
		compiling_list = outer_lists.back();
		outer_lists.pop_back();
		return;
	}
	compiling = false;
	compiling_list = 0;
	st.bound_texture = binding_before_lists;
}

void dgl_glCallList(GLuint list)
{
	PASS(glCallList(list));
	if (record([list] { dgl_glCallList(list); })) return;
	auto it = lists.find(list);
	if (it == lists.end()) return;
	for (auto& f : it->second) f();
}

// ---- Queries

const GLubyte* dgl_glGetString(GLenum name)
{
	PASS_RET(glGetString(name));
	static std::string renderer;
	switch (name) {
		case GL_VENDOR: return reinterpret_cast<const GLubyte*>("Apple (Durandal Metal)");
		case GL_RENDERER:
			renderer = device ? std::string(device.name.UTF8String) + " (Metal)" : "Metal";
			return reinterpret_cast<const GLubyte*>(renderer.c_str());
		case GL_VERSION: return reinterpret_cast<const GLubyte*>("2.1 Durandal Metal");
		case GL_EXTENSIONS:
			// What the engine checks for; sRGB is left out so it stays off, as by default
			return reinterpret_cast<const GLubyte*>("GL_ARB_vertex_shader GL_ARB_fragment_shader GL_ARB_shader_objects "
				"GL_ARB_shading_language_100 GL_EXT_framebuffer_object GL_ARB_texture_non_power_of_two "
				"GL_ARB_texture_compression GL_EXT_texture_compression_s3tc GL_EXT_texture_filter_anisotropic "
				"GL_SGIS_generate_mipmap");
		default: return reinterpret_cast<const GLubyte*>("");
	}
}

void dgl_glGetIntegerv(GLenum pname, GLint* p)
{
	PASS(glGetIntegerv(pname, p));
	switch (pname) {
		case GL_VIEWPORT: std::copy(st.viewport, st.viewport + 4, p); break;
		case GL_SCISSOR_BOX: std::copy(st.scissor_box, st.scissor_box + 4, p); break;
		case GL_MAX_TEXTURE_SIZE: *p = 16384; break;
		case GL_MATRIX_MODE: *p = st.matrix_mode; break;
		case GL_STENCIL_BITS: *p = 8; break;
		default: *p = 0; break;
	}
}

void dgl_glGetFloatv(GLenum pname, GLfloat* p)
{
	PASS(glGetFloatv(pname, p));
	const simd_float4x4* m = nullptr;
	switch (pname) {
		case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT: *p = 16; return;
		case GL_MODELVIEW_MATRIX: m = &modelview_stack.back(); break;
		case GL_PROJECTION_MATRIX: m = &projection_stack.back(); break;
		case GL_TEXTURE_MATRIX: m = &texture_stack.back(); break;
		default: *p = 0; return;
	}
	for (int c = 0; c < 4; ++c)
		for (int r = 0; r < 4; ++r) p[c * 4 + r] = m->columns[c][r];
}

void dgl_glGetDoublev(GLenum pname, GLdouble* p)
{
	PASS(glGetDoublev(pname, p));
	GLfloat f[16] = {0};
	dgl_glGetFloatv(pname, f);
	const int n = (pname == GL_MODELVIEW_MATRIX || pname == GL_PROJECTION_MATRIX || pname == GL_TEXTURE_MATRIX) ? 16 : 1;
	for (int i = 0; i < n; ++i) p[i] = f[i];
}

void dgl_glBlitFramebufferEXT(GLint a, GLint b, GLint c, GLint d, GLint e, GLint f, GLint g, GLint h, GLbitfield mask, GLenum filter)
{
	PASS(glBlitFramebufferEXT(a, b, c, d, e, f, g, h, mask, filter));
	// Only movie export uses this; it is not available in Metal display mode yet
}
