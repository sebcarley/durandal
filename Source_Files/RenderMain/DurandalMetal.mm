/*
	DurandalMetal.mm — Durandal project

	See DurandalMetal.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#define GL_SILENCE_DEPRECATION

// Apple's frameworks first: engine headers (cseries.h) define macros that
// CarbonCore's headers do not expect.
#import <Metal/Metal.h>
#if !__has_feature(objc_arc)
#error "DurandalMetal.mm must be compiled with ARC"
#endif
#import <IOSurface/IOSurface.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

#include "DurandalMetal.h"
#include "DurandalMetalShaders.h"
#include "DurandalGL.h"
#include "DurandalPreferences.h"
#include "DurandalShading.h"
#include "DurandalBenchmark.h"

#include "cseries.h"
#include "ImageLoader.h"
#include "Logging.h"
#include "OGL_Setup.h"
#include "OGL_Textures.h"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <memory>
#include <unordered_map>

bool OGL_IsActive();

namespace DurandalMetal {

namespace {

// Development: DURANDAL_GPU_TIMING=1 timestamps each stage of every frame
// on the GPU (stage-boundary counters) and, at exit, writes per-frame rows
// and a summary next to the benchmark CSV (<csv>.gpu.csv, <csv>.gpu.txt)
// and to stderr.
namespace timing {

enum Stage { kVolume, kBake, kAverage, kWorld, kAO, kBloom, kCanvas, kOutput, kStages };
const char* const kStageNames[kStages] = { "fog volume", "light bake", "light averages", "world pass",
	"ambient shadows", "bloom", "blit and 2D", "output" };
const char* const kStageColumns[kStages] = { "volume_ms", "bake_ms", "average_ms", "world_ms",
	"ao_ms", "bloom_ms", "canvas_ms", "output_ms" };
const bool kRenderStage[kStages] = { false, false, false, true, true, true, true, true };
// Small passes after the world are timed from their fragment work's start:
// on a tile GPU their vertex stage starts early and waits behind the world
// pass, which counted that wait (4-7 ms each). The world pass keeps vertex
// start, as it always has, so its numbers compare with earlier rounds.
const bool kFromFragment[kStages] = { false, false, false, false, true, true, true, true };
constexpr int kSlots = 4;			// frames in flight, with room to spare
constexpr int kPerStage = 4;		// world: vertex start/end, fragment start/end
constexpr int kPerSlot = kStages * kPerStage;

struct Row {
	size_t bench_frame;			// the benchmark's index for this frame
	double total_ms;
	uint64_t ticks[kStages];	// 0: stage not run
};

id<MTLCounterSampleBuffer> samples;
int slot = 0;
bool used[kStages];
bool frame_open = false;	// between begin_frame and end_frame: a frame with a world
std::mutex mutex;
std::vector<Row> rows;
MTLTimestamp cpu0 = 0, gpu0 = 0;

bool enabled()
{
	static const bool on = getenv("DURANDAL_GPU_TIMING") != nullptr;
	return on;
}

void report();

bool ready(id<MTLDevice> device)
{
	if (!enabled())
		return false;
	if (samples)
		return true;
	static bool tried = false;
	if (tried)
		return false;
	tried = true;
	id<MTLCounterSet> timestamps = nil;
	for (id<MTLCounterSet> set in device.counterSets)
		if ([set.name isEqualToString:MTLCommonCounterSetTimestamp])
			timestamps = set;
	if (!timestamps || ![device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary])
		return false;
	MTLCounterSampleBufferDescriptor* d = [MTLCounterSampleBufferDescriptor new];
	d.counterSet = timestamps;
	d.storageMode = MTLStorageModeShared;
	d.sampleCount = kSlots * kPerSlot;
	NSError* error = nil;
	samples = [device newCounterSampleBufferWithDescriptor:d error:&error];
	if (!samples)
		return false;
	[device sampleTimestamps:&cpu0 gpuTimestamp:&gpu0];
	std::atexit(report);
	return true;
}

NSUInteger index(Stage stage, int k)
{
	return NSUInteger(slot * kPerSlot + stage * kPerStage + k);
}

void begin_frame(id<MTLDevice> device)
{
	if (!ready(device))
		return;
	slot = (slot + 1) % kSlots;
	std::fill(std::begin(used), std::end(used), false);
	frame_open = true;
}

// A compute encoder, timestamped when timing
id<MTLComputeCommandEncoder> compute(id<MTLCommandBuffer> cb, Stage stage)
{
	if (!samples)
		return [cb computeCommandEncoder];
	MTLComputePassDescriptor* d = [MTLComputePassDescriptor computePassDescriptor];
	d.sampleBufferAttachments[0].sampleBuffer = samples;
	d.sampleBufferAttachments[0].startOfEncoderSampleIndex = index(stage, 0);
	d.sampleBufferAttachments[0].endOfEncoderSampleIndex = index(stage, 1);
	used[stage] = true;
	return [cb computeCommandEncoderWithDescriptor:d];
}

// A stage of several passes (bloom) takes its start from the first and its
// end from the last
void attach(MTLRenderPassDescriptor* pass, Stage stage, bool first = true, bool last = true)
{
	if (!samples || !frame_open)
		return;
	pass.sampleBufferAttachments[0].sampleBuffer = samples;
	pass.sampleBufferAttachments[0].startOfVertexSampleIndex = first && !kFromFragment[stage] ? index(stage, 0) : MTLCounterDontSample;
	pass.sampleBufferAttachments[0].endOfVertexSampleIndex = MTLCounterDontSample;
	pass.sampleBufferAttachments[0].startOfFragmentSampleIndex = first && kFromFragment[stage] ? index(stage, 2) : MTLCounterDontSample;
	pass.sampleBufferAttachments[0].endOfFragmentSampleIndex = last ? index(stage, 3) : MTLCounterDontSample;
	used[stage] = true;
}

void end_frame(id<MTLCommandBuffer> cb)
{
	if (!samples || !frame_open)
		return;
	frame_open = false;
	const int this_slot = slot;
	const size_t bench_frame = DurandalBenchmark::NextFrameIndex();
	uint32_t this_used = 0;	// a bit per stage (blocks cannot capture arrays)
	for (int st = 0; st < kStages; ++st)
		if (used[st])
			this_used |= 1u << st;
	[cb addCompletedHandler:^(id<MTLCommandBuffer> done) {
		NSData* data = [samples resolveCounterRange:NSMakeRange(this_slot * kPerSlot, kPerSlot)];
		if (!data)
			return;
		const MTLCounterResultTimestamp* t = (const MTLCounterResultTimestamp*)data.bytes;
		Row row = {};
		row.bench_frame = bench_frame;
		row.total_ms = (done.GPUEndTime - done.GPUStartTime) * 1000.0;
		for (int st = 0; st < kStages; ++st) {
			if (!(this_used & (1u << st)))
				continue;
			const uint64_t a = t[st * kPerStage + (kFromFragment[st] ? 2 : 0)].timestamp;
			const uint64_t b = t[st * kPerStage + (kRenderStage[st] ? 3 : 1)].timestamp;
			if (a == MTLCounterErrorValue || b == MTLCounterErrorValue || b <= a)
				continue;
			row.ticks[st] = b - a;
		}
		std::lock_guard<std::mutex> lock(mutex);
		rows.push_back(row);
	}];
}

void report()
{
	std::lock_guard<std::mutex> lock(mutex);
	if (rows.empty() || !samples)
		return;
	// GPU timestamp ticks to milliseconds, from two CPU/GPU samples
	MTLTimestamp cpu1 = 0, gpu1 = 0;
	[samples.device sampleTimestamps:&cpu1 gpuTimestamp:&gpu1];
	const double cpu_ns = double(cpu1 - cpu0);	// sampleTimestamps' CPU side is in nanoseconds
	const double ms_per_tick = gpu1 > gpu0 ? cpu_ns / double(gpu1 - gpu0) / 1e6 : 1e-6;

	const char* csv = getenv("DURANDAL_BENCHMARK");
	FILE* f = csv ? fopen((std::string(csv) + ".gpu.csv").c_str(), "w") : nullptr;
	if (f) {
		fprintf(f, "frame,bench_frame,total_ms");
		for (int st = 0; st < kStages; ++st)
			fprintf(f, ",%s", kStageColumns[st]);
		fprintf(f, "\n");
	}
	std::vector<double> series[kStages + 1];
	for (size_t i = 0; i < rows.size(); ++i) {
		const Row& r = rows[i];
		series[kStages].push_back(r.total_ms);
		if (f)
			fprintf(f, "%zu,%zu,%.3f", i, r.bench_frame, r.total_ms);
		for (int st = 0; st < kStages; ++st) {
			const double ms = r.ticks[st] * ms_per_tick;
			series[st].push_back(ms);
			if (f)
				fprintf(f, ",%.3f", ms);
		}
		if (f)
			fprintf(f, "\n");
	}
	if (f)
		fclose(f);

	FILE* txt = csv ? fopen((std::string(csv) + ".gpu.txt").c_str(), "w") : nullptr;
	auto line = [&](const char* name, std::vector<double> v) {
		std::sort(v.begin(), v.end());
		double sum = 0;
		for (double x : v) sum += x;
		const double avg = sum / v.size();
		const double p99 = v[std::min(v.size() - 1, size_t(v.size() * 0.99))];
		char buffer[160];
		snprintf(buffer, sizeof(buffer), "  %-15s average %6.2f ms   p99 %6.2f ms   max %6.2f ms\n", name, avg, p99, v.back());
		fputs(buffer, stderr);
		if (txt)
			fputs(buffer, txt);
	};
	fprintf(stderr, "GPU time per frame (%zu frames):\n", rows.size());
	if (txt)
		fprintf(txt, "GPU time per frame (%zu frames):\n", rows.size());
	line("whole frame", series[kStages]);
	for (int st = 0; st < kStages; ++st)
		line(kStageNames[st], series[st]);
	if (txt)
		fclose(txt);
}

}

bool init_attempted = false;
bool init_ok = false;

id<MTLDevice> device;
id<MTLCommandQueue> queue;
id<MTLLibrary> library;
const MTLPixelFormat kGlowFormat = MTLPixelFormatRGBA16Float;
// Each surface's distance from the viewer, kept in tile memory only (W1)
const MTLPixelFormat kDistanceFormat = MTLPixelFormatR32Float;
const double kFarDistance = 1e9;

// [program][blend][may discard]: solid surfaces use variant 0 (see kMayDiscard)
typedef __strong id<MTLRenderPipelineState> PipelineSet[kNumberOfPrograms][5][2];
PipelineSet pipelines;
PipelineSet msaa_pipelines;		// 4x, for anti-aliased edges (L4); built on first use
bool msaa_attempted = false, msaa_ok = false;
const int kMSAASamples = 4;
id<MTLRenderPipelineState> gamma_pipeline;
id<MTLDepthStencilState> depth_states[2][2];	// [test][write]
std::unordered_map<uint32_t, id<MTLSamplerState>> samplers;

// Frame resources
int target_width = 0, target_height = 0;
id<MTLTexture> world_color;		// persistent, so the void can smear
id<MTLTexture> world_depth;
// Anti-aliased edges (L4): 4x memoryless targets, resolved into
// world_color. For the void smear, the previous frame (world_previous,
// swapped with world_color each frame) is drawn in first as the backdrop.
id<MTLTexture> msaa_color;
id<MTLTexture> msaa_depth;
id<MTLTexture> msaa_glow;
id<MTLTexture> msaa_distance;
id<MTLTexture> world_distance;	// stored for the ambient shadows (Round 12), else write-only

// Ambient shadows (Round 12)
id<MTLTexture> ao_texture;
id<MTLRenderPipelineState> ao_pipeline;
float ao_far_reach = 0;			// set by run_ao for the blit
bool ao_dev_view = false;
bool ao_attempted = false, ao_ok = false;
bool frame_ao = false;
bool viewer_under_liquid = false;	// Round 12: no ambient shadows under a liquid
float frame_distance_shade = 0;	// Round 12: 0..1
simd_float4x4 view_projection_matrix, view_inverse_matrix;
simd_float4 view_camera;
// Glow (E1): the world's emissive light, half float; when the frame has no
// glow the pass writes a memoryless scratch target instead
id<MTLTexture> world_glow;
id<MTLTexture> glow_scratch;
bool frame_glow = false;		// the pass writes world_glow
bool frame_glow_edr = false;	// shown above SDR white (Glow + HDR output)
bool frame_bloom = false;		// bloom from it (Bloom)
// Bloom: half resolution, kBloomLevels mips; built on first use
const int kBloomLevels = 5;
id<MTLTexture> bloom;
id<MTLRenderPipelineState> bloom_down_pipeline, bloom_up_pipeline;
bool bloom_attempted = false, bloom_ok = false;
id<MTLTexture> world_previous;
id<MTLRenderPipelineState> backdrop_pipeline;
bool frame_msaa = false;
IOSurfaceRef output_surface = nullptr;
id<MTLTexture> output_texture;	// on output_surface
GLuint output_gl_texture = 0;

id<MTLCommandBuffer> command_buffer;
id<MTLRenderCommandEncoder> encoder;
bool in_world_pass = false;
// The world pass's render encoder opens at its first draw (or EndWorld),
// so compute work (the fog volume) can run first in the same frame
MTLRenderPassDescriptor* pending_pass;
bool pending_backdrop = false;
// What the world's draws read, kept to bind when the encoder opens
Light bound_lights[kMaximumLights];
int bound_light_count = 0;
Caster bound_casters[kMaximumCasters];
int bound_caster_count = 0;
id<MTLBuffer> bound_map;
id<MTLBuffer> no_map;			// stands in when no map is set

// Volumetric fog (V1)
id<MTLComputePipelineState> volume_pipeline;
bool volume_attempted = false, volume_ok = false;
id<MTLTexture> volume_texture;	// columns x rows x slices: in-scatter, transmittance
id<MTLTexture> volume_none;		// 1x1x1: no fog
id<MTLTexture> current_volume;

// Light redistribution (E4): the surface cache
id<MTLComputePipelineState> bake_pipeline, average_pipeline;
bool radiance_attempted = false, radiance_ok = false;
id<MTLTexture> radiance_atlas;		// lumels: radiance seen, alpha 1 where the surface is
id<MTLTexture> radiance_previous;	// Bounced Light (R4): the atlas as it was before this frame's bake
id<MTLBuffer> surface_patch_buffer;	// Bounced Light: int per polygon x 10 (floor, ceiling, edges): its patch, -1 none
int surface_patch_polygons = 0;		//   polygons it covers
id<MTLBuffer> patch_buffer;			// Patch per patch
id<MTLBuffer> average_buffer;		// float4 per patch: average radiance, lumel count (of its group)
id<MTLBuffer> group_start_buffer;	// per group: first index into group_member_buffer (plus one at the end)
id<MTLBuffer> group_member_buffer;	// patch indices, grouped
int patch_count = 0;
id<MTLTexture> no_radiance;			// 1x1: nothing baked
id<MTLBuffer> no_patch;

// Traced shadows (R1)
id<MTLBuffer> bound_occluders, bound_occluder_polygons, bound_occluder_indices;
id<MTLTexture> mask_array;			// kMaskSlices silhouettes, R8, mipmapped
id<MTLTexture> no_masks;			// 1x1x1: none

// Air that moves: the motes' pipelines, single and multisampled
id<MTLRenderPipelineState> mote_pipeline, mote_pipeline_msaa;
bool motes_attempted = false;

// Living water (R2): ripple sources, entry 0 the count
simd_float4 bound_ripples[kMaximumRipples + 1];

// Traced ambient shadows (R3)
id<MTLBuffer> grid_cells, grid_indices;
simd_float4 grid_header = { 0, 0, 1, 0 };
int grid_columns = 0, grid_rows = 0;
bool traced_ambient = false;
int traced_ambient_polygons = 0;
int traced_ambient_viewer = -1;		// the viewer's polygon

// The level's surfaces for traced rays (reflecting liquids, R2)
id<MTLBuffer> bound_surfaces;
id<MTLTexture> colour_array;		// kColourSlices wall textures, RGBA8, mipmapped
id<MTLTexture> sky_texture;			// the landscape, RGBA8, mipmapped
id<MTLTexture> no_colours;			// 1x1x1 array: none
id<MTLTexture> no_sky;				// 1x1: none

// Development capture (parity check)
int force_mode = -1;
bool capture_requested = false;
bool capture_ready = false;
std::vector<uint8_t> capture_pixels;
int capture_width = 0, capture_height = 0;

// Bound by UseTexture/PlaceTexture for the next Draw()
id<MTLTexture> current_texture;
id<MTLSamplerState> current_sampler;
uint32_t current_sampler_key = 0;
// HD art: a pack's normal map (TextureState::Bump), bound beside the
// colour; a flat one stands in when there is none
id<MTLTexture> current_bump;
id<MTLTexture> flat_bump;

// 3D pickups: per-model vertex and index buffers, and skin textures
struct ModelBuffers {
	id<MTLBuffer> vertices;
	id<MTLBuffer> indices;
	int vertex_count = 0;
	int index_count = 0;
};
std::unordered_map<const void*, ModelBuffers> model_buffers;
std::unordered_map<const void*, id<MTLTexture>> model_skins;
std::unordered_map<const void*, uint32_t> model_skin_samplers;

// 8-bit shading: bound by BindRamps; ramps textures per colour table,
// rebuilt when the colour environment changes
id<MTLTexture> current_indices;
id<MTLTexture> current_ramps;

// Textures with no transparent texel (set when each is placed)
std::unordered_map<const void*, bool> opaque_textures;

bool is_opaque(id<MTLTexture> t)
{
	auto it = opaque_textures.find((__bridge const void*)t);
	return it != opaque_textures.end() && it->second;
}
std::unordered_map<uint32_t, id<MTLTexture>> ramp_textures;
uint32_t ramp_generation = 0;

bool is_ramp_program(Program p)
{
	return p == kWallRamp || p == kSpriteRamp;
}

// The 8-bit shading walk (build_shading_tables8; see DurandalShading.h),
// evaluated per pixel value for every table, 256 x kShadeRows RGBA8:
// rows 0-31 the 32 tables exactly (0 darkest), then kSmoothRows rows of
// the same walk at fractional tables, blending adjacent ramp colours.
const int kSmoothRows = 128;
const int kShadeRows = 32 + kSmoothRows;

// Glow (E1), in the shades' alpha: 255 for self-luminous colours (monster
// eyes and armour lights, which glow whatever the light), else 0. Walls'
// lights are per texel, in the index texture (DurandalGlow.h).
void bake_shades(const DurandalShading::Ramps& r, std::vector<uint8_t>& out)
{
	const int n = DurandalShading::kLevels;	// 32
	out.assign(size_t(256) * kShadeRows * 4, 0);
	auto entry = [&](int start, int pos, int count, int channel) -> float {
		return pos >= count ? 0.f : r.data[1][std::min(start + pos, 255)][channel];
	};
	for (int value = 0; value < 256; ++value)
	{
		const uint8_t* info = r.data[0][value];
		const int start = info[0], i = info[1], count = info[2];
		const int span = count + (info[3] & 1);
		const bool self_luminous = (info[3] & 0x80) != 0;
		const uint8_t mask = self_luminous ? 255 : 0;
		for (int row = 0; row < kShadeRows; ++row)
		{
			uint8_t* px = &out[(size_t(row) * 256 + value) * 4];
			px[3] = mask;
			if (row < n)
			{
				// Banded: table `row`, level counted from the brightest
				int level = n - 1 - row;
				if (self_luminous)
					level >>= 1;
				const int pos = i + (level * (span - i)) / (n - 1);
				for (int c = 0; c < 3; ++c)
					px[c] = uint8_t(entry(start, pos, count, c));
			}
			else
			{
				const float t = float(row - n) * float(n - 1) / float(kSmoothRows - 1);
				float level = float(n - 1) - t;
				if (self_luminous)
					level *= 0.5f;
				const float pos = float(i) + level * float(span - i) / float(n - 1);
				const int p0 = int(std::floor(pos));
				const float f = pos - float(p0);
				for (int c = 0; c < 3; ++c)
				{
					const float v = entry(start, p0, count, c) * (1 - f) + entry(start, p0 + 1, count, c) * f;
					px[c] = uint8_t(std::lround(std::min(255.f, v)));
				}
			}
		}
	}
}

const char* kFragmentNames[kNumberOfPrograms] = {
	"wall_fragment",
	"wall_infravision_fragment",
	"sprite_fragment",
	"sprite_infravision_fragment",
	"invincible_fragment",
	"invisible_fragment",
	"landscape_fragment",
	"landscape_infravision_fragment",
	"landscape_sphere_fragment",
	"landscape_sphere_infravision_fragment",
	"wall_ramp_fragment",
	"sprite_ramp_fragment",
	"liquid_fragment",
	"shadow_fragment",
};

void set_blend(MTLRenderPipelineColorAttachmentDescriptor* a, int blend)
{
	if (blend == kBlendNone)
	{
		a.blendingEnabled = NO;
		return;
	}
	a.blendingEnabled = YES;
	MTLBlendFactor src = MTLBlendFactorSourceAlpha, dst = MTLBlendFactorOneMinusSourceAlpha;
	switch (blend)
	{
		case kBlendCrossfade: src = MTLBlendFactorSourceAlpha; dst = MTLBlendFactorOneMinusSourceAlpha; break;
		case kBlendAdd: src = MTLBlendFactorSourceAlpha; dst = MTLBlendFactorOne; break;
		case kBlendCrossfadePremultiplied: src = MTLBlendFactorOne; dst = MTLBlendFactorOneMinusSourceAlpha; break;
		case kBlendAddPremultiplied: src = MTLBlendFactorOne; dst = MTLBlendFactorOne; break;
	}
	// glBlendFunc applies the same factors to colour and alpha
	a.sourceRGBBlendFactor = a.sourceAlphaBlendFactor = src;
	a.destinationRGBBlendFactor = a.destinationAlphaBlendFactor = dst;
}

bool build_pipelines(PipelineSet& set, int samples)
{
	NSError* error = nil;
	id<MTLFunction> world_vertex = [library newFunctionWithName:@"world_vertex"];
	for (int p = 0; p < kNumberOfPrograms; ++p)
	for (int discard = 0; discard < 2; ++discard)
	{
		MTLFunctionConstantValues* constants = [MTLFunctionConstantValues new];
		const bool may_discard = discard != 0;
		[constants setConstantValue:&may_discard type:MTLDataTypeBool atIndex:0];
		id<MTLFunction> fragment = [library newFunctionWithName:[NSString stringWithUTF8String:kFragmentNames[p]]
												 constantValues:constants error:&error];
		if (!fragment)
		{
			logError("Durandal Metal: shader %s failed: %s", kFragmentNames[p], error.localizedDescription.UTF8String);
			return false;
		}
		// Only unblended draws can be solid
		for (int b = discard ? 0 : kBlendNone; b < 5; ++b)
		{
			MTLRenderPipelineDescriptor* d = [MTLRenderPipelineDescriptor new];
			d.vertexFunction = world_vertex;
			d.fragmentFunction = fragment;
			d.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
			set_blend(d.colorAttachments[0], b);
			d.colorAttachments[1].pixelFormat = kGlowFormat;	// glow (E1), blended alike
			set_blend(d.colorAttachments[1], b);
			d.colorAttachments[2].pixelFormat = kDistanceFormat;	// distance (W1)
			if (b != kBlendNone)
			{
				// Round 12: a blended draw's distance is written where the
				// fragment says so (WorldFrag: alpha 1 or 0), whatever its
				// colour blend, so a sprite's transparent texels leave the
				// distance behind them
				MTLRenderPipelineColorAttachmentDescriptor* a = d.colorAttachments[2];
				a.blendingEnabled = YES;
				a.sourceRGBBlendFactor = a.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
				a.destinationRGBBlendFactor = a.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
			}
			if (p == kLiquid)
			{
				// Liquids compose over what is below themselves; the distance
				// they write (negated) marks the surface (Round 12)
				set_blend(d.colorAttachments[0], kBlendNone);
				set_blend(d.colorAttachments[1], kBlendNone);
			}
			d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
			d.rasterSampleCount = samples;
			set[p][b][discard] = [device newRenderPipelineStateWithDescriptor:d error:&error];
			if (!set[p][b][discard])
			{
				logError("Durandal Metal: pipeline %s (%dx) failed: %s", kFragmentNames[p], samples, error.localizedDescription.UTF8String);
				return false;
			}
		}
	}
	return true;
}

bool init()
{
	if (init_attempted)
		return init_ok;
	init_attempted = true;

	@autoreleasepool {
		device = MTLCreateSystemDefaultDevice();
		if (!device)
		{
			logError("Durandal Metal: no Metal device");
			return false;
		}
		queue = [device newCommandQueue];

		NSError* error = nil;
		MTLCompileOptions* options = [MTLCompileOptions new];
		if (@available(macOS 15.0, *))
			options.mathMode = MTLMathModeSafe;	// match GLSL's IEEE behaviour
		else
			options.fastMathEnabled = NO;
		library = [device newLibraryWithSource:[NSString stringWithUTF8String:kDurandalMetalShaderSource]
									   options:options error:&error];
		if (!library)
		{
			logError("Durandal Metal: shader compile failed: %s", error.localizedDescription.UTF8String);
			return false;
		}

		if (!build_pipelines(pipelines, 1))
			return false;

		MTLRenderPipelineDescriptor* g = [MTLRenderPipelineDescriptor new];
		g.vertexFunction = [library newFunctionWithName:@"blit_vertex"];
		g.fragmentFunction = [library newFunctionWithName:@"gamma_fragment"];
		g.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
		gamma_pipeline = [device newRenderPipelineStateWithDescriptor:g error:&error];
		if (!gamma_pipeline)
		{
			logError("Durandal Metal: gamma pipeline failed: %s", error.localizedDescription.UTF8String);
			return false;
		}

		for (int t = 0; t < 2; ++t)
			for (int w = 0; w < 2; ++w)
			{
				MTLDepthStencilDescriptor* d = [MTLDepthStencilDescriptor new];
				d.depthCompareFunction = t ? MTLCompareFunctionLess : MTLCompareFunctionAlways;
				// GL doesn't write depth while the depth test is disabled
				d.depthWriteEnabled = (t && w) ? YES : NO;
				depth_states[t][w] = [device newDepthStencilStateWithDescriptor:d];
			}

		no_map = [device newBufferWithLength:sizeof(simd_float4) * 9 options:MTLResourceStorageModeShared];
		std::memset(no_map.contents, 0, no_map.length);
		MTLTextureDescriptor* vd = [MTLTextureDescriptor new];
		vd.textureType = MTLTextureType3D;
		vd.pixelFormat = MTLPixelFormatRGBA16Float;
		vd.width = vd.height = vd.depth = 1;
		vd.usage = MTLTextureUsageShaderRead;
		volume_none = [device newTextureWithDescriptor:vd];
		const uint16_t clear[4] = { 0, 0, 0, 0x3c00 };	// in-scatter 0, transmittance 1
		[volume_none replaceRegion:MTLRegionMake3D(0, 0, 0, 1, 1, 1) mipmapLevel:0 slice:0 withBytes:clear
					   bytesPerRow:sizeof(clear) bytesPerImage:sizeof(clear)];

		MTLTextureDescriptor* rd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
																					  width:1 height:1 mipmapped:NO];
		rd.usage = MTLTextureUsageShaderRead;
		no_radiance = [device newTextureWithDescriptor:rd];
		const uint16_t nothing[4] = { 0, 0, 0, 0 };
		[no_radiance replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:nothing bytesPerRow:sizeof(nothing)];
		no_patch = [device newBufferWithLength:sizeof(Patch) options:MTLResourceStorageModeShared];
		std::memset(no_patch.contents, 0, no_patch.length);
		MTLTextureDescriptor* md = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																					  width:1 height:1 mipmapped:NO];
		md.textureType = MTLTextureType2DArray;
		md.arrayLength = 1;
		md.usage = MTLTextureUsageShaderRead;
		no_masks = [device newTextureWithDescriptor:md];
		const uint8_t clear_mask[4] = { 0, 0, 0, 0 };
		[no_masks replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 slice:0 withBytes:clear_mask
					bytesPerRow:4 bytesPerImage:4];
		MTLTextureDescriptor* cd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																					  width:1 height:1 mipmapped:NO];
		cd.usage = MTLTextureUsageShaderRead;
		no_sky = [device newTextureWithDescriptor:cd];
		cd.textureType = MTLTextureType2DArray;
		cd.arrayLength = 1;
		no_colours = [device newTextureWithDescriptor:cd];
		const uint8_t grey[4] = { 128, 128, 128, 255 };
		[no_sky replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:grey bytesPerRow:4];
		[no_colours replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 slice:0 withBytes:grey
					  bytesPerRow:4 bytesPerImage:4];

		logNote("Durandal Metal: initialised on %s", device.name.UTF8String);
		init_ok = true;
	}
	return init_ok;
}

void release_targets()
{
	world_color = nil;
	world_depth = nil;
	msaa_color = nil;
	msaa_depth = nil;
	msaa_glow = nil;
	msaa_distance = nil;
	world_distance = nil;
	world_glow = nil;
	glow_scratch = nil;
	bloom = nil;
	world_previous = nil;
	output_texture = nil;
	if (output_gl_texture)
	{
		glDeleteTextures(1, &output_gl_texture);
		output_gl_texture = 0;
	}
	if (output_surface)
	{
		CFRelease(output_surface);
		output_surface = nullptr;
	}
	target_width = target_height = 0;
}

bool ensure_targets(int w, int h)
{
	if (w == target_width && h == target_height && world_color)
		return true;
	release_targets();

	MTLTextureDescriptor* cd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:w height:h mipmapped:NO];
	cd.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	cd.storageMode = MTLStorageModePrivate;
	world_color = [device newTextureWithDescriptor:cd];

	MTLTextureDescriptor* dd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:w height:h mipmapped:NO];
	dd.usage = MTLTextureUsageRenderTarget;
	dd.storageMode = MTLStorageModePrivate;
	world_depth = [device newTextureWithDescriptor:dd];

	MTLTextureDescriptor* gd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:kGlowFormat width:w height:h mipmapped:NO];
	gd.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	gd.storageMode = MTLStorageModePrivate;
	world_glow = [device newTextureWithDescriptor:gd];
	gd.usage = MTLTextureUsageRenderTarget;
	gd.storageMode = MTLStorageModeMemoryless;
	glow_scratch = [device newTextureWithDescriptor:gd];
	MTLTextureDescriptor* xd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:kDistanceFormat width:w height:h mipmapped:NO];
	xd.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	xd.storageMode = MTLStorageModePrivate;
	world_distance = [device newTextureWithDescriptor:xd];
	ao_texture = nil;

	if (DurandalGL::Active())
	{
		// Metal display: the world is composited straight into the drawable
		if (!world_color || !world_depth)
			return false;
		target_width = w;
		target_height = h;
		return true;
	}

	NSDictionary* props = @{
		(id)kIOSurfaceWidth: @(w),
		(id)kIOSurfaceHeight: @(h),
		(id)kIOSurfaceBytesPerElement: @4,
		(id)kIOSurfacePixelFormat: @((uint32_t)'BGRA'),
	};
	output_surface = IOSurfaceCreate((__bridge CFDictionaryRef)props);
	if (!output_surface)
	{
		logError("Durandal Metal: IOSurfaceCreate failed");
		return false;
	}
	MTLTextureDescriptor* od = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:w height:h mipmapped:NO];
	od.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	od.storageMode = MTLStorageModeShared;
	output_texture = [device newTextureWithDescriptor:od iosurface:output_surface plane:0];

	glGenTextures(1, &output_gl_texture);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, output_gl_texture);
	CGLError err = CGLTexImageIOSurface2D(CGLGetCurrentContext(), GL_TEXTURE_RECTANGLE_ARB, GL_RGBA, w, h,
										  GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, output_surface, 0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	if (!world_color || !world_depth || !output_texture || err != kCGLNoError)
	{
		logError("Durandal Metal: could not create %dx%d targets (CGL error %d)", w, h, (int)err);
		release_targets();
		return false;
	}

	target_width = w;
	target_height = h;
	return true;
}

// Anti-aliased edges: pipelines (once) and targets at the world's size
bool ensure_msaa(int w, int h, bool& fresh)
{
	if (!msaa_attempted)
	{
		msaa_attempted = true;
		msaa_ok = [device supportsTextureSampleCount:kMSAASamples] && build_pipelines(msaa_pipelines, kMSAASamples);
		if (msaa_ok)
		{
			NSError* error = nil;
			MTLRenderPipelineDescriptor* d = [MTLRenderPipelineDescriptor new];
			d.vertexFunction = [library newFunctionWithName:@"blit_vertex"];
			d.fragmentFunction = [library newFunctionWithName:@"backdrop_fragment"];
			d.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
			d.colorAttachments[1].pixelFormat = kGlowFormat;
			d.colorAttachments[1].writeMask = MTLColorWriteMaskNone;
			d.colorAttachments[2].pixelFormat = kDistanceFormat;
			d.colorAttachments[2].writeMask = MTLColorWriteMaskNone;
			d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
			d.rasterSampleCount = kMSAASamples;
			backdrop_pipeline = [device newRenderPipelineStateWithDescriptor:d error:&error];
			msaa_ok = backdrop_pipeline != nil;
		}
		if (!msaa_ok)
			logWarning("Durandal Metal: anti-aliased edges unavailable");
	}
	if (!msaa_ok)
		return false;
	fresh = false;
	if (msaa_color && msaa_color.width == NSUInteger(w) && msaa_color.height == NSUInteger(h))
		return true;
	fresh = true;
	MTLTextureDescriptor* cd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:w height:h mipmapped:NO];
	cd.textureType = MTLTextureType2DMultisample;
	cd.sampleCount = kMSAASamples;
	cd.usage = MTLTextureUsageRenderTarget;
	cd.storageMode = MTLStorageModeMemoryless;
	msaa_color = [device newTextureWithDescriptor:cd];
	MTLTextureDescriptor* pd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:w height:h mipmapped:NO];
	pd.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	pd.storageMode = MTLStorageModePrivate;
	world_previous = [device newTextureWithDescriptor:pd];
	MTLTextureDescriptor* dd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:w height:h mipmapped:NO];
	dd.textureType = MTLTextureType2DMultisample;
	dd.sampleCount = kMSAASamples;
	dd.usage = MTLTextureUsageRenderTarget;
	dd.storageMode = MTLStorageModeMemoryless;
	msaa_depth = [device newTextureWithDescriptor:dd];
	MTLTextureDescriptor* gd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:kGlowFormat width:w height:h mipmapped:NO];
	gd.textureType = MTLTextureType2DMultisample;
	gd.sampleCount = kMSAASamples;
	gd.usage = MTLTextureUsageRenderTarget;
	gd.storageMode = MTLStorageModeMemoryless;
	msaa_glow = [device newTextureWithDescriptor:gd];
	MTLTextureDescriptor* xd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:kDistanceFormat width:w height:h mipmapped:NO];
	xd.textureType = MTLTextureType2DMultisample;
	xd.sampleCount = kMSAASamples;
	xd.usage = MTLTextureUsageRenderTarget;
	xd.storageMode = MTLStorageModeMemoryless;
	msaa_distance = [device newTextureWithDescriptor:xd];
	return msaa_color && msaa_depth && msaa_glow && msaa_distance && world_previous;
}

// Bloom (E1): pipelines (once) and the half-resolution mip chain
bool ensure_bloom(int w, int h)
{
	if (!bloom_attempted)
	{
		bloom_attempted = true;
		NSError* error = nil;
		MTLRenderPipelineDescriptor* d = [MTLRenderPipelineDescriptor new];
		d.vertexFunction = [library newFunctionWithName:@"blit_vertex"];
		d.fragmentFunction = [library newFunctionWithName:@"bloom_down_fragment"];
		d.colorAttachments[0].pixelFormat = kGlowFormat;
		bloom_down_pipeline = [device newRenderPipelineStateWithDescriptor:d error:&error];
		d.fragmentFunction = [library newFunctionWithName:@"bloom_up_fragment"];
		d.colorAttachments[0].blendingEnabled = YES;
		d.colorAttachments[0].sourceRGBBlendFactor = d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
		d.colorAttachments[0].destinationRGBBlendFactor = d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
		bloom_up_pipeline = [device newRenderPipelineStateWithDescriptor:d error:&error];
		bloom_ok = bloom_down_pipeline && bloom_up_pipeline;
		if (!bloom_ok)
			logWarning("Durandal Metal: bloom unavailable: %s", error.localizedDescription.UTF8String);
	}
	if (!bloom_ok)
		return false;
	const int bw = std::max(1, w / 2), bh = std::max(1, h / 2);
	if (bloom && bloom.width == NSUInteger(bw) && bloom.height == NSUInteger(bh))
		return true;
	MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:kGlowFormat width:bw height:bh mipmapped:YES];
	d.mipmapLevelCount = std::min<NSUInteger>(kBloomLevels, d.mipmapLevelCount);
	d.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	d.storageMode = MTLStorageModePrivate;
	bloom = [device newTextureWithDescriptor:d];
	return bloom != nil;
}

// Ambient shadows (Round 12): pipelines (once) and the occlusion image
bool ensure_ao(int w, int h)
{
	if (!ao_attempted)
	{
		ao_attempted = true;
		NSError* error = nil;
		MTLRenderPipelineDescriptor* d = [MTLRenderPipelineDescriptor new];
		d.vertexFunction = [library newFunctionWithName:@"blit_vertex"];
		d.fragmentFunction = [library newFunctionWithName:@"ao_fragment"];
		d.colorAttachments[0].pixelFormat = MTLPixelFormatR8Unorm;
		ao_pipeline = [device newRenderPipelineStateWithDescriptor:d error:&error];
		ao_ok = ao_pipeline != nil;
		if (!ao_ok)
			logWarning("Durandal Metal: ambient shadows unavailable: %s", error.localizedDescription.UTF8String);
	}
	if (!ao_ok)
		return false;
	// Half resolution: the apply pass's depth-aware blur hides the steps
	const int aw = std::max(1, w / 2), ah = std::max(1, h / 2);
	if (ao_texture && ao_texture.width == NSUInteger(aw) && ao_texture.height == NSUInteger(ah))
		return true;
	MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:aw height:ah mipmapped:NO];
	d.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	d.storageMode = MTLStorageModePrivate;
	ao_texture = [device newTextureWithDescriptor:d];
	return ao_texture != nil;
}

// The occlusion image from the distance image; the world blit
// (DurandalGL::DrawWorldImage) blurs it and darkens the world by it.
// Development: DURANDAL_AO="radius,strength,far" (world units; 512, 1.2,
// 24576), DURANDAL_AO_VIEW=1 shows the occlusion.
void run_ao()
{
	// Must match struct AOParams in the shaders
	struct AOParams {
		simd_float4x4 view_projection;
		simd_float4x4 inverse;
		simd_float4 camera;
		simd_float4 screen;
		float radius, strength, far, view;
		simd_float4 grid;		// traced (R3): the polygon grid's origin, cells per world unit
		simd_int4 traced;		//   x on, y columns, z rows, w polygons in the occluder lists
		simd_int4 viewer;		//   x the viewer's polygon
	} params;
	static float radius = 512, strength = 1.2f, far = 24 * 1024;
	// DURANDAL_AO_VIEW=2 or 3 (traced, R3): only the map's surfaces, or only the figures
	static const int ao_view = getenv("DURANDAL_AO_VIEW") ? std::max(1, std::atoi(getenv("DURANDAL_AO_VIEW"))) : 0;
	static bool parsed = false;
	if (!parsed)
	{
		parsed = true;
		if (const char* v = getenv("DURANDAL_AO"))
			sscanf(v, "%f,%f,%f", &radius, &strength, &far);
	}
	params.view_projection = view_projection_matrix;
	params.inverse = view_inverse_matrix;
	params.camera = view_camera;
	params.screen = simd_make_float4(1.0f / ao_texture.width, 1.0f / ao_texture.height, ao_texture.width, ao_texture.height);
	params.radius = radius;
	params.strength = strength;
	params.far = far;
	params.view = float(ao_view);
	const bool traced = traced_ambient && bound_map && grid_cells && grid_indices;
	params.grid = grid_header;
	params.traced = simd_make_int4(traced ? 1 : 0, grid_columns, grid_rows, traced ? traced_ambient_polygons : 0);
	params.viewer = simd_make_int4(traced_ambient_viewer, 0, 0, 0);
	ao_far_reach = far;
	ao_dev_view = ao_view != 0;

	MTLRenderPassDescriptor* p = [MTLRenderPassDescriptor renderPassDescriptor];
	p.colorAttachments[0].texture = ao_texture;
	p.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	p.colorAttachments[0].storeAction = MTLStoreActionStore;
	timing::attach(p, timing::kAO);
	id<MTLRenderCommandEncoder> e = [command_buffer renderCommandEncoderWithDescriptor:p];
	[e setRenderPipelineState:ao_pipeline];
	[e setFragmentBytes:&params length:sizeof(params) atIndex:0];
	[e setFragmentTexture:world_distance atIndex:0];
	// Traced (R3): the map, the grid and the figures
	const bool occluders = traced && traced_ambient_polygons > 0 && bound_occluders && bound_occluder_polygons && bound_occluder_indices;
	[e setFragmentBuffer:(traced ? bound_map : no_map) offset:0 atIndex:1];
	[e setFragmentBuffer:(traced ? grid_cells : no_patch) offset:0 atIndex:2];
	[e setFragmentBuffer:(traced ? grid_indices : no_patch) offset:0 atIndex:3];
	[e setFragmentBuffer:(occluders ? bound_occluders : no_patch) offset:0 atIndex:4];
	[e setFragmentBuffer:(occluders ? bound_occluder_polygons : no_patch) offset:0 atIndex:5];
	[e setFragmentBuffer:(occluders ? bound_occluder_indices : no_patch) offset:0 atIndex:6];
	[e setFragmentTexture:(mask_array ? mask_array : no_masks) atIndex:1];
	[e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
	[e endEncoding];

}

// Downsample the glow image through the chain, then add each level's
// tent-filtered upsample into the one above; level 0 ends up as the bloom
void run_bloom()
{
	struct { simd_float2 texel; int first; float radius; } params;
	const NSUInteger levels = bloom.mipmapLevelCount;
	int passes_left = int(levels) * 2 - 1;
	auto pass_into = [&](NSUInteger level, bool load) {
		MTLRenderPassDescriptor* p = [MTLRenderPassDescriptor renderPassDescriptor];
		p.colorAttachments[0].texture = bloom;
		p.colorAttachments[0].level = level;
		p.colorAttachments[0].loadAction = load ? MTLLoadActionLoad : MTLLoadActionDontCare;
		p.colorAttachments[0].storeAction = MTLStoreActionStore;
		const bool first = passes_left == int(levels) * 2 - 1;
		if (first || passes_left == 1)
			timing::attach(p, timing::kBloom, first, passes_left == 1);
		--passes_left;
		return [command_buffer renderCommandEncoderWithDescriptor:p];
	};
	for (NSUInteger level = 0; level < levels; ++level)
	{
		id<MTLTexture> src = level == 0 ? world_glow : [bloom newTextureViewWithPixelFormat:kGlowFormat textureType:MTLTextureType2D
																				 levels:NSMakeRange(level - 1, 1) slices:NSMakeRange(0, 1)];
		params.texel = simd_make_float2(1.0f / src.width, 1.0f / src.height);
		params.first = level == 0 ? 1 : 0;
		params.radius = 1;
		id<MTLRenderCommandEncoder> e = pass_into(level, false);
		[e setRenderPipelineState:bloom_down_pipeline];
		[e setFragmentBytes:&params length:sizeof(params) atIndex:0];
		[e setFragmentTexture:src atIndex:0];
		[e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
		[e endEncoding];
	}
	for (NSUInteger level = levels - 1; level > 0; --level)
	{
		id<MTLTexture> src = [bloom newTextureViewWithPixelFormat:kGlowFormat textureType:MTLTextureType2D
														   levels:NSMakeRange(level, 1) slices:NSMakeRange(0, 1)];
		params.texel = simd_make_float2(1.0f / src.width, 1.0f / src.height);
		params.first = 0;
		params.radius = 1;
		id<MTLRenderCommandEncoder> e = pass_into(level - 1, true);
		[e setRenderPipelineState:bloom_up_pipeline];
		[e setFragmentBytes:&params length:sizeof(params) atIndex:0];
		[e setFragmentTexture:src atIndex:0];
		[e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
		[e endEncoding];
	}
}

// Draws the IOSurface into the current OpenGL viewport, as the shader
// renderer's FBO blit would.
void draw_output_in_gl()
{
	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT);
	glUseProgram(0);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_FOG);
	glDisable(GL_TEXTURE_2D);
	for (int i = 0; i < 6; ++i)
		glDisable(GL_CLIP_PLANE0 + i);

	glEnable(GL_TEXTURE_RECTANGLE_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, output_gl_texture);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f(1, 1, 1, 1);

	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	// The IOSurface's first row is the top of the Metal image; GL puts
	// a texture's first row at t = 0, so the top edge samples t = 0.
	const float w = target_width, h = target_height;
	glBegin(GL_QUADS);
	glTexCoord2f(0, h); glVertex2f(-1, -1);
	glTexCoord2f(w, h); glVertex2f(1, -1);
	glTexCoord2f(w, 0); glVertex2f(1, 1);
	glTexCoord2f(0, 0); glVertex2f(-1, 1);
	glEnd();

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	glPopAttrib();
}

MTLSamplerAddressMode address_for(int mode)
{
	switch (mode)
	{
		case 1: return MTLSamplerAddressModeMirrorRepeat;
		case 2: return MTLSamplerAddressModeClampToEdge;
		default: return MTLSamplerAddressModeRepeat;
	}
}

// key: mag(1) min(1) mip(2) wrap_s(2) wrap_t(2) aniso(5)
id<MTLSamplerState> sampler_for(uint32_t key)
{
	auto it = samplers.find(key);
	if (it != samplers.end())
		return it->second;
	MTLSamplerDescriptor* d = [MTLSamplerDescriptor new];
	d.magFilter = (key & 1) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
	d.minFilter = (key & 2) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
	switch ((key >> 2) & 3)
	{
		case 1: d.mipFilter = MTLSamplerMipFilterNearest; break;
		case 2: d.mipFilter = MTLSamplerMipFilterLinear; break;
		default: d.mipFilter = MTLSamplerMipFilterNotMipmapped; break;
	}
	d.sAddressMode = address_for((key >> 4) & 3);
	d.tAddressMode = address_for((key >> 6) & 3);
	d.maxAnisotropy = std::max<uint32_t>(1, (key >> 8) & 31);
	id<MTLSamplerState> s = [device newSamplerStateWithDescriptor:d];
	samplers[key] = s;
	return s;
}


// Crisp filtering (L4): linear magnification (the shader keeps texels
// sharp), trilinear minification where there are mipmaps, 16x anisotropic;
// wrapping as before
uint32_t crisp_key(uint32_t key, bool mipmapped)
{
	return 1u | (1u << 1) | ((mipmapped ? 2u : 0u) << 2) | (key & (0xFu << 4)) | (16u << 8);
}

}

bool WorldRendererActive()
{
	if (DurandalGL::Active())	// Metal display: the world is always Metal
		return init();
	// Otherwise only the development bridge (parity check) uses it; the
	// setting itself takes effect at launch, as the Metal display
	return force_mode == 1 && OGL_IsActive() && init();
}

bool DisplayActive()
{
	return DurandalGL::Active();
}

void ForceWorldRenderer(int mode)
{
	force_mode = mode;
}

void RequestCapture()
{
	capture_requested = true;
	capture_ready = false;
}

bool CaptureRequested()
{
	return capture_requested;
}

void CaptureGLFramebuffer(int w, int h)
{
	if (!capture_requested || w <= 0 || h <= 0)
		return;
	capture_requested = false;
	capture_pixels.resize(size_t(w) * h * 4);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, capture_pixels.data());
	// GL rows run bottom-up; make them top-down like Metal's
	std::vector<uint8_t> row(size_t(w) * 4);
	for (int y = 0; y < h / 2; ++y)
	{
		uint8_t* a = capture_pixels.data() + size_t(y) * w * 4;
		uint8_t* b = capture_pixels.data() + size_t(h - 1 - y) * w * 4;
		std::copy(a, a + row.size(), row.data());
		std::copy(b, b + row.size(), a);
		std::copy(row.data(), row.data() + row.size(), b);
	}
	capture_width = w;
	capture_height = h;
	capture_ready = true;
}

bool TakeCapture(std::vector<uint8_t>& rgba, int& width, int& height)
{
	if (!capture_ready)
		return false;
	rgba.swap(capture_pixels);
	width = capture_width;
	height = capture_height;
	capture_ready = false;
	return true;
}

namespace {

void bind_world_inputs()
{
	Light no_light[1] = {};
	if (bound_light_count)
		[encoder setFragmentBytes:bound_lights length:sizeof(Light) * bound_light_count atIndex:2];
	else
		[encoder setFragmentBytes:no_light length:sizeof(no_light) atIndex:2];
	[encoder setFragmentBuffer:(bound_map ? bound_map : no_map) offset:0 atIndex:3];
	Caster no_caster[1] = {};
	if (bound_caster_count)
		[encoder setFragmentBytes:bound_casters length:sizeof(Caster) * bound_caster_count atIndex:4];
	else
		[encoder setFragmentBytes:no_caster length:sizeof(no_caster) atIndex:4];
	[encoder setFragmentTexture:current_volume atIndex:3];
	const bool radiance = radiance_atlas && patch_count;
	[encoder setFragmentBuffer:(radiance ? patch_buffer : no_patch) offset:0 atIndex:5];
	[encoder setFragmentBuffer:(radiance ? average_buffer : no_patch) offset:0 atIndex:6];
	[encoder setFragmentTexture:(radiance ? radiance_atlas : no_radiance) atIndex:4];
	// Traced shadows (R1)
	const bool occluders = bound_occluders && bound_occluder_polygons && bound_occluder_indices;
	[encoder setFragmentBuffer:(occluders ? bound_occluders : no_patch) offset:0 atIndex:7];
	[encoder setFragmentBuffer:(occluders ? bound_occluder_polygons : no_patch) offset:0 atIndex:8];
	[encoder setFragmentBuffer:(occluders ? bound_occluder_indices : no_patch) offset:0 atIndex:9];
	[encoder setFragmentTexture:(mask_array ? mask_array : no_masks) atIndex:6];
	// Reflecting liquids (R2)
	[encoder setFragmentBuffer:(bound_surfaces ? bound_surfaces : no_patch) offset:0 atIndex:10];
	[encoder setFragmentTexture:(colour_array ? colour_array : no_colours) atIndex:7];
	[encoder setFragmentTexture:(sky_texture ? sky_texture : no_sky) atIndex:8];
	[encoder setFragmentBytes:bound_ripples length:sizeof(simd_float4) * (int(bound_ripples[0].x) + 1) atIndex:11];
}

// Opens the world pass's render encoder if it is not open yet
bool open_world_encoder()
{
	if (encoder)
		return true;
	if (!in_world_pass || !pending_pass)
		return false;
	timing::attach(pending_pass, timing::kWorld);
	encoder = [command_buffer renderCommandEncoderWithDescriptor:pending_pass];
	pending_pass = nil;
	if (pending_backdrop)
	{
		[encoder setRenderPipelineState:backdrop_pipeline];
		[encoder setDepthStencilState:depth_states[0][0]];
		[encoder setFragmentTexture:world_previous atIndex:0];
		[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
	}
	// As OGL_StartMain(): cull back faces, clockwise is front
	[encoder setFrontFacingWinding:MTLWindingClockwise];
	[encoder setCullMode:MTLCullModeBack];
	bind_world_inputs();
	return true;
}

bool ensure_radiance()
{
	if (radiance_attempted)
		return radiance_ok;
	radiance_attempted = true;
	NSError* error = nil;
	id<MTLFunction> bake = [library newFunctionWithName:@"radiance_bake"];
	id<MTLFunction> average = [library newFunctionWithName:@"radiance_average"];
	bake_pipeline = bake ? [device newComputePipelineStateWithFunction:bake error:&error] : nil;
	average_pipeline = average ? [device newComputePipelineStateWithFunction:average error:&error] : nil;
	if (!bake_pipeline || !average_pipeline)
		logError("Durandal Metal: surface cache pipelines failed: %s", error ? error.localizedDescription.UTF8String : "no function");
	if (device.readWriteTextureSupport < MTLReadWriteTextureTier2)
	{
		logError("Durandal Metal: surface cache needs read-write textures (tier 2)");
		bake_pipeline = average_pipeline = nil;
	}
	radiance_ok = bake_pipeline && average_pipeline;
	return radiance_ok;
}

bool ensure_volume()
{
	if (volume_attempted)
		return volume_ok;
	volume_attempted = true;
	NSError* error = nil;
	id<MTLFunction> f = [library newFunctionWithName:@"volume_kernel"];
	volume_pipeline = f ? [device newComputePipelineStateWithFunction:f error:&error] : nil;
	if (!volume_pipeline)
		logError("Durandal Metal: fog volume pipeline failed: %s", error ? error.localizedDescription.UTF8String : "no function");
	volume_ok = volume_pipeline != nil;
	return volume_ok;
}

}

void SetLights(const Light* list, int count)
{
	bound_light_count = std::clamp(count, 0, int(kMaximumLights));
	if (bound_light_count)
		std::memcpy(bound_lights, list, sizeof(Light) * bound_light_count);
	if (in_world_pass && encoder)
		bind_world_inputs();
}

void SetCasters(const Caster* list, int count)
{
	bound_caster_count = std::clamp(count, 0, int(kMaximumCasters));
	if (bound_caster_count)
		std::memcpy(bound_casters, list, sizeof(Caster) * bound_caster_count);
	if (in_world_pass && encoder)
		bind_world_inputs();
}

void SetMap(const simd_float4* map, int polygon_count)
{
	if (!in_world_pass || !map || polygon_count <= 0)
	{
		bound_map = nil;
		return;
	}
	static id<MTLBuffer> ring[3];
	static int next = 0;
	const size_t bytes = sizeof(simd_float4) * 9 * polygon_count;
	__strong id<MTLBuffer>& b = ring[next];
	next = (next + 1) % 3;	// at most two frames in flight
	if (!b || b.length < bytes)
		b = [device newBufferWithLength:bytes * 2 options:MTLResourceStorageModeShared];
	std::memcpy(b.contents, map, bytes);
	bound_map = b;
	if (encoder)
		bind_world_inputs();
}

void SetRadianceLayout(const std::vector<Patch>& patches, int atlas_width, int atlas_height,
					   const std::vector<int>& group_start, const std::vector<int>& group_members)
{
	if (!init() || !ensure_radiance())
		return;
	@autoreleasepool {
		patch_count = int(patches.size());
		radiance_atlas = nil;
		radiance_previous = nil;
		surface_patch_buffer = nil;
		if (!patch_count || atlas_width <= 0 || atlas_height <= 0 || group_start.size() < 2 || group_members.empty())
			return;
		// Bounced Light: where a ray's hit finds its patch (kind 0 floor, 1
		// ceiling, 2 wall along edge info.z of polygon info.y)
		int polygons = 0;
		for (const Patch& pa : patches)
			polygons = std::max(polygons, int(pa.info.y) + 1);
		std::vector<int32_t> lookup(size_t(std::max(polygons, 1)) * 10, -1);
		for (size_t i = 0; i < patches.size(); ++i)
		{
			const simd_int4 info = patches[i].info;
			const int part = info.x == 2 ? 2 + info.z : info.x;
			if (info.y >= 0 && part >= 0 && part < 10)
				lookup[size_t(info.y) * 10 + part] = int32_t(i);
		}
		surface_patch_buffer = [device newBufferWithBytes:lookup.data() length:sizeof(int32_t) * lookup.size()
												  options:MTLResourceStorageModeShared];
		surface_patch_polygons = polygons;
		patch_buffer = [device newBufferWithBytes:patches.data() length:sizeof(Patch) * patches.size()
										  options:MTLResourceStorageModeShared];
		group_start_buffer = [device newBufferWithBytes:group_start.data() length:sizeof(int) * group_start.size()
												options:MTLResourceStorageModeShared];
		group_member_buffer = [device newBufferWithBytes:group_members.data() length:sizeof(int) * group_members.size()
												 options:MTLResourceStorageModeShared];
		// Averages start at zero lumels: nothing applied until baked
		average_buffer = [device newBufferWithLength:sizeof(simd_float4) * patches.size() options:MTLResourceStorageModePrivate];
		MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
																					 width:atlas_width height:atlas_height mipmapped:NO];
		d.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
		d.storageMode = MTLStorageModePrivate;
		radiance_atlas = [device newTextureWithDescriptor:d];
		d.usage = MTLTextureUsageShaderRead;
		radiance_previous = [device newTextureWithDescriptor:d];
		id<MTLCommandBuffer> cb = [queue commandBuffer];
		id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
		[blit fillBuffer:average_buffer range:NSMakeRange(0, average_buffer.length) value:0];
		[blit endEncoding];
		[cb commit];
		logNote("Durandal Metal: surface cache %d patches, atlas %dx%d", patch_count, atlas_width, atlas_height);
	}
}

bool RadianceReady()
{
	return radiance_atlas && patch_count;
}

bool RunRadiance(const std::vector<BakeTile>& tiles, const std::vector<int>& baked,
				 const std::vector<simd_float4>& surfaces, int rays, uint32_t seed,
				 bool bounce, simd_float4 range)
{
	if (!in_world_pass || encoder || !RadianceReady() || !bound_map || tiles.empty() || surfaces.empty())
		return false;
	@autoreleasepool {
		static id<MTLBuffer> ring[3][3];	// tiles, baked, surfaces; two frames in flight
		static int next = 0;
		auto upload = [&](int which, const void* data, size_t bytes) {
			__strong id<MTLBuffer>& b = ring[next][which];
			if (!b || b.length < bytes)
				b = [device newBufferWithLength:std::max<size_t>(bytes * 2, 256) options:MTLResourceStorageModeShared];
			std::memcpy(b.contents, data, bytes);
			return b;
		};
		id<MTLBuffer> tile_buffer = upload(0, tiles.data(), sizeof(BakeTile) * tiles.size());
		id<MTLBuffer> baked_buffer = upload(1, baked.data(), sizeof(int) * baked.size());
		id<MTLBuffer> surface_buffer = upload(2, surfaces.data(), sizeof(simd_float4) * surfaces.size());
		next = (next + 1) % 3;

		// Must match struct BakeParams in the shaders
		struct { uint32_t rays; uint32_t seed; uint32_t bounce; uint32_t polygons; simd_float4 range; } params =
			{ uint32_t(rays), seed, bounce && radiance_previous && surface_patch_buffer ? 1u : 0u,
			  uint32_t(surface_patch_polygons), range };
		if (params.bounce)
		{
			// The bounce reads the lumels as they were: the bake writes the
			// atlas while other lumels read it
			id<MTLBlitCommandEncoder> copy = [command_buffer blitCommandEncoder];
			[copy copyFromTexture:radiance_atlas toTexture:radiance_previous];
			[copy endEncoding];
		}
		id<MTLComputeCommandEncoder> c = timing::compute(command_buffer, timing::kBake);
		[c setComputePipelineState:bake_pipeline];
		[c setBytes:&params length:sizeof(params) atIndex:0];
		[c setBuffer:tile_buffer offset:0 atIndex:1];
		[c setBuffer:patch_buffer offset:0 atIndex:2];
		[c setBuffer:bound_map offset:0 atIndex:3];
		[c setBuffer:surface_buffer offset:0 atIndex:4];
		[c setBuffer:surface_patch_buffer offset:0 atIndex:5];
		[c setBuffer:average_buffer offset:0 atIndex:6];
		[c setTexture:radiance_atlas atIndex:0];
		[c setTexture:radiance_previous atIndex:1];
		[c dispatchThreadgroups:MTLSizeMake(tiles.size(), 1, 1) threadsPerThreadgroup:MTLSizeMake(kBakeTile, kBakeTile, 1)];
		[c endEncoding];

		if (!baked.empty())
		{
			c = timing::compute(command_buffer, timing::kAverage);
			[c setComputePipelineState:average_pipeline];
			[c setBuffer:baked_buffer offset:0 atIndex:0];
			[c setBuffer:patch_buffer offset:0 atIndex:1];
			[c setBuffer:average_buffer offset:0 atIndex:2];
			[c setBuffer:group_start_buffer offset:0 atIndex:3];
			[c setBuffer:group_member_buffer offset:0 atIndex:4];
			[c setTexture:radiance_atlas atIndex:0];
			[c dispatchThreadgroups:MTLSizeMake(baked.size(), 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
			[c endEncoding];
		}
	}
	return true;
}

bool RunVolume(VolumeParams params, simd_float4& screen)
{
	if (!in_world_pass || encoder || !ensure_volume() || target_width <= 0 || target_height <= 0)
		return false;
	if (!(bound_occluders && bound_occluder_polygons && bound_occluder_indices))
		params.figures = 0;	// no figures this frame: the kernel must not read the lists
	// Slices from a quarter of a world unit to 64 world units, spaced
	// evenly in log distance (each about a tenth deeper than the last)
	const float nearest = 256.0f, farthest = 65536.0f;
	params.nearest = nearest;
	params.slice_scale = float(kVolumeSlices - 1) / std::log(farthest / nearest);
	screen = simd_make_float4(1.0f / target_width, 1.0f / target_height, params.slice_scale, nearest);
	@autoreleasepool {
		const int columns = (target_width + kVolumeCell - 1) / kVolumeCell;
		const int rows = (target_height + kVolumeCell - 1) / kVolumeCell;
		if (!volume_texture || int(volume_texture.width) != columns || int(volume_texture.height) != rows)
		{
			MTLTextureDescriptor* d = [MTLTextureDescriptor new];
			d.textureType = MTLTextureType3D;
			d.pixelFormat = MTLPixelFormatRGBA16Float;
			d.width = columns;
			d.height = rows;
			d.depth = kVolumeSlices;
			d.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
			d.storageMode = MTLStorageModePrivate;
			volume_texture = [device newTextureWithDescriptor:d];
			if (!volume_texture)
				return false;
		}
		params.grid = simd_make_uint4(columns, rows, kVolumeSlices, bound_light_count);
		id<MTLComputeCommandEncoder> c = timing::compute(command_buffer, timing::kVolume);
		[c setComputePipelineState:volume_pipeline];
		[c setBytes:&params length:sizeof(params) atIndex:0];
		Light no_light[1] = {};
		if (bound_light_count)
			[c setBytes:bound_lights length:sizeof(Light) * bound_light_count atIndex:1];
		else
			[c setBytes:no_light length:sizeof(no_light) atIndex:1];
		[c setBuffer:(bound_map ? bound_map : no_map) offset:0 atIndex:2];
		[c setTexture:volume_texture atIndex:0];
		// Shafts (R1): the figures near the lights
		const bool occluders = params.figures > 0 && bound_occluders && bound_occluder_polygons && bound_occluder_indices;
		[c setBuffer:(occluders ? bound_occluders : no_patch) offset:0 atIndex:3];
		[c setBuffer:(occluders ? bound_occluder_polygons : no_patch) offset:0 atIndex:4];
		[c setBuffer:(occluders ? bound_occluder_indices : no_patch) offset:0 atIndex:5];
		[c setTexture:(mask_array ? mask_array : no_masks) atIndex:1];
		[c dispatchThreads:MTLSizeMake(columns, rows, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
		[c endEncoding];
		current_volume = volume_texture;
	}
	return true;
}

void SetView(simd_float4x4 view_projection, simd_float4x4 inverse, simd_float4 camera)
{
	view_projection_matrix = view_projection;
	view_inverse_matrix = inverse;
	view_camera = camera;
}

void SetOccluders(const Occluder* list, int count, const simd_int2* polygons, int polygon_count,
				  const int* indices, int index_count)
{
	if (!in_world_pass || count <= 0 || polygon_count <= 0 || index_count <= 0)
	{
		bound_occluders = bound_occluder_polygons = bound_occluder_indices = nil;
		if (encoder)
			bind_world_inputs();
		return;
	}
	static id<MTLBuffer> ring[3][3];	// occluders, polygon lists, indices; two frames in flight
	static int next = 0;
	auto upload = [&](int which, const void* data, size_t bytes) {
		__strong id<MTLBuffer>& b = ring[next][which];
		if (!b || b.length < bytes)
			b = [device newBufferWithLength:std::max<size_t>(bytes * 2, 256) options:MTLResourceStorageModeShared];
		std::memcpy(b.contents, data, bytes);
		return b;
	};
	bound_occluders = upload(0, list, sizeof(Occluder) * std::min(count, kMaximumOccluders));
	bound_occluder_polygons = upload(1, polygons, sizeof(simd_int2) * polygon_count);
	bound_occluder_indices = upload(2, indices, sizeof(int) * index_count);
	next = (next + 1) % 3;
	if (encoder)
		bind_world_inputs();
}

bool SetMask(int slice, const std::vector<std::vector<uint8_t>>& levels)
{
	if (!init() || slice < 0 || slice >= kMaskSlices || levels.empty())
		return false;
	@autoreleasepool {
		if (!mask_array)
		{
			MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																						 width:kMaskSize height:kMaskSize mipmapped:YES];
			d.textureType = MTLTextureType2DArray;
			d.arrayLength = kMaskSlices;
			d.usage = MTLTextureUsageShaderRead;
			d.storageMode = MTLStorageModeShared;
			mask_array = [device newTextureWithDescriptor:d];
			if (!mask_array)
				return false;
		}
		int size = kMaskSize;
		for (size_t level = 0; level < levels.size() && level < mask_array.mipmapLevelCount && size >= 1; ++level, size /= 2)
		{
			if (levels[level].size() < size_t(size) * size * 4)
				break;
			[mask_array replaceRegion:MTLRegionMake2D(0, 0, size, size) mipmapLevel:level slice:slice
							withBytes:levels[level].data() bytesPerRow:size * 4 bytesPerImage:size_t(size) * size * 4];
		}
	}
	return true;
}

static id<MTLRenderPipelineState> build_mote_pipeline(int samples)
{
	NSError* error = nil;
	MTLRenderPipelineDescriptor* d = [MTLRenderPipelineDescriptor new];
	d.vertexFunction = [library newFunctionWithName:@"mote_vertex"];
	d.fragmentFunction = [library newFunctionWithName:@"mote_fragment"];
	// Composed in the shader over what is drawn (it reads the attachments)
	d.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
	d.colorAttachments[1].pixelFormat = kGlowFormat;
	d.colorAttachments[2].pixelFormat = kDistanceFormat;
	d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
	d.rasterSampleCount = samples;
	id<MTLRenderPipelineState> p = [device newRenderPipelineStateWithDescriptor:d error:&error];
	if (!p)
		logWarning("Durandal Metal: dust and embers unavailable: %s", error ? error.localizedDescription.UTF8String : "no function");
	return p;
}

void DrawMotes(const Mote* motes, int count, const Uniforms& uniforms)
{
	if (!in_world_pass || count <= 0 || !motes)
		return;
	if (!motes_attempted)
	{
		motes_attempted = true;
		mote_pipeline = build_mote_pipeline(1);
		mote_pipeline_msaa = build_mote_pipeline(kMSAASamples);
	}
	id<MTLRenderPipelineState> pipeline = frame_msaa ? mote_pipeline_msaa : mote_pipeline;
	static const bool log = getenv("DURANDAL_AIR_LOG") != nullptr;
	static bool logged = false;
	if (log && !logged)
	{
		logged = true;
		fprintf(stderr, "Durandal air: pipelines %d/%d, msaa %d, %d motes\n", mote_pipeline != nil, mote_pipeline_msaa != nil, frame_msaa, count);
	}
	if (!pipeline || !open_world_encoder())
		return;
	count = std::min(count, kMaximumMotes);
	static id<MTLBuffer> ring[3];
	static int next = 0;
	const size_t bytes = sizeof(Mote) * count;
	__strong id<MTLBuffer>& b = ring[next];
	next = (next + 1) % 3;	// at most two frames in flight
	if (!b || b.length < bytes)
		b = [device newBufferWithLength:sizeof(Mote) * kMaximumMotes options:MTLResourceStorageModeShared];
	std::memcpy(b.contents, motes, bytes);
	[encoder setRenderPipelineState:pipeline];
	[encoder setDepthStencilState:depth_states[0][0]];
	[encoder setCullMode:MTLCullModeNone];
	[encoder setVertexBytes:&uniforms length:sizeof(Uniforms) atIndex:1];
	[encoder setFragmentBytes:&uniforms length:sizeof(Uniforms) atIndex:1];
	[encoder setVertexBuffer:b offset:0 atIndex:12];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:count];
	[encoder setCullMode:MTLCullModeBack];
}

void SetRipples(const simd_float4* sources, int count)
{
	count = std::clamp(count, 0, kMaximumRipples);
	bound_ripples[0] = simd_make_float4(count, 0, 0, 0);
	if (count)
		std::memcpy(&bound_ripples[1], sources, sizeof(simd_float4) * count);
	if (in_world_pass && encoder)
		bind_world_inputs();
}

void SetPolygonGrid(simd_float4 header, int columns, int rows, const std::vector<simd_int2>& cells,
					const std::vector<int>& indices)
{
	if (!init() || cells.empty() || indices.empty() || columns <= 0 || rows <= 0)
		return;
	grid_cells = [device newBufferWithBytes:cells.data() length:sizeof(simd_int2) * cells.size()
									options:MTLResourceStorageModeShared];
	grid_indices = [device newBufferWithBytes:indices.data() length:sizeof(int) * indices.size()
									  options:MTLResourceStorageModeShared];
	grid_header = header;
	grid_columns = columns;
	grid_rows = rows;
}

void SetTracedAmbient(bool traced, int occluder_polygons, int viewer_polygon)
{
	traced_ambient = traced;
	traced_ambient_polygons = traced ? occluder_polygons : 0;
	traced_ambient_viewer = viewer_polygon;
}

void SetSurfaces(const simd_float4* table, int count)
{
	if (!in_world_pass || !table || count <= 0)
	{
		bound_surfaces = nil;
		if (encoder)
			bind_world_inputs();
		return;
	}
	static id<MTLBuffer> ring[3];
	static int next = 0;
	const size_t bytes = sizeof(simd_float4) * count;
	__strong id<MTLBuffer>& b = ring[next];
	next = (next + 1) % 3;	// at most two frames in flight
	if (!b || b.length < bytes)
		b = [device newBufferWithLength:bytes * 2 options:MTLResourceStorageModeShared];
	std::memcpy(b.contents, table, bytes);
	bound_surfaces = b;
	if (encoder)
		bind_world_inputs();
}

bool SetSurfaceColour(int slice, const std::vector<std::vector<uint8_t>>& levels)
{
	if (!init() || slice < 0 || slice >= kColourSlices || levels.empty())
		return false;
	@autoreleasepool {
		if (!colour_array)
		{
			MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																						 width:kColourSize height:kColourSize mipmapped:YES];
			d.textureType = MTLTextureType2DArray;
			d.arrayLength = kColourSlices;
			d.usage = MTLTextureUsageShaderRead;
			d.storageMode = MTLStorageModeShared;
			colour_array = [device newTextureWithDescriptor:d];
			if (!colour_array)
				return false;
		}
		int size = kColourSize;
		for (size_t level = 0; level < levels.size() && level < colour_array.mipmapLevelCount && size >= 1; ++level, size /= 2)
		{
			if (levels[level].size() < size_t(size) * size * 4)
				break;
			[colour_array replaceRegion:MTLRegionMake2D(0, 0, size, size) mipmapLevel:level slice:slice
							  withBytes:levels[level].data() bytesPerRow:size * 4 bytesPerImage:size_t(size) * size * 4];
		}
	}
	return true;
}

bool SetSky(const std::vector<std::vector<uint8_t>>& levels, int width, int height)
{
	if (!init() || levels.empty() || width <= 0 || height <= 0)
		return false;
	@autoreleasepool {
		MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																					 width:width height:height mipmapped:YES];
		d.usage = MTLTextureUsageShaderRead;
		d.storageMode = MTLStorageModeShared;
		id<MTLTexture> t = [device newTextureWithDescriptor:d];
		if (!t)
			return false;
		int w = width, h = height;
		for (size_t level = 0; level < levels.size() && level < t.mipmapLevelCount; ++level)
		{
			if (levels[level].size() < size_t(w) * h * 4)
				break;
			[t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:level withBytes:levels[level].data() bytesPerRow:w * 4];
			w = std::max(1, w / 2);
			h = std::max(1, h / 2);
		}
		sky_texture = t;
	}
	return true;
}

void TimeDisplayPass(void* pass, TimedDisplayPass which)
{
	timing::attach((__bridge MTLRenderPassDescriptor*)pass, which == kTimedCanvas ? timing::kCanvas : timing::kOutput);
}

void EndFrameTiming(void* command_buffer)
{
	timing::end_frame((__bridge id<MTLCommandBuffer>)command_buffer);
}

void SetViewerUnderLiquid(bool under)
{
	viewer_under_liquid = under;
}

bool GlowActive()
{
	return in_world_pass && frame_glow;
}

bool InWorldPass()
{
	return in_world_pass;
}

bool BeginWorld(int pixel_width, int pixel_height, bool keep_previous_frame, simd_float4 clear_color)
{
	if (!init() || pixel_width <= 0 || pixel_height <= 0)
		return false;
	@autoreleasepool {
		const bool fresh = !(pixel_width == target_width && pixel_height == target_height && world_color);
		if (!ensure_targets(pixel_width, pixel_height))
			return false;

		if (DurandalGL::Active())
		{
			// Metal display: same command buffer as the rest of the frame
			command_buffer = (__bridge id<MTLCommandBuffer>)DurandalGL::FrameCommandBuffer();
			DurandalGL::EndScreenPass();
		}
		else
			command_buffer = [queue commandBuffer];
		MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
		bool msaa_fresh = false;
		frame_msaa = Durandal::Enabled(Durandal::kEdgeSmoothing) && ensure_msaa(pixel_width, pixel_height, msaa_fresh);
		bool backdrop = false;
		if (frame_msaa)
		{
			backdrop = keep_previous_frame && !fresh;
			if (backdrop)
				std::swap(world_color, world_previous);
			pass.colorAttachments[0].texture = msaa_color;
			pass.colorAttachments[0].resolveTexture = world_color;
			pass.colorAttachments[0].loadAction = MTLLoadActionClear;
			pass.colorAttachments[0].storeAction = MTLStoreActionMultisampleResolve;
			pass.depthAttachment.texture = msaa_depth;
		}
		else
		{
			pass.colorAttachments[0].texture = world_color;
			pass.colorAttachments[0].loadAction = (keep_previous_frame && !fresh) ? MTLLoadActionLoad : MTLLoadActionClear;
			pass.colorAttachments[0].storeAction = MTLStoreActionStore;
			pass.depthAttachment.texture = world_depth;
		}
		pass.colorAttachments[0].clearColor = MTLClearColorMake(clear_color.x, clear_color.y, clear_color.z, clear_color.w);

		// Glow (E1): only in Metal display mode, shown above white with Glow
		// on and HDR showing, and feeding Bloom
		frame_glow_edr = DurandalGL::Active() && Durandal::Enabled(Durandal::kGlow) && DurandalGL::GlowWanted();
		frame_bloom = DurandalGL::Active() && Durandal::Enabled(Durandal::kBloom) && ensure_bloom(pixel_width, pixel_height);
		frame_glow = frame_glow_edr || frame_bloom;
		pass.colorAttachments[1].clearColor = MTLClearColorMake(0, 0, 0, 0);
		pass.colorAttachments[1].loadAction = MTLLoadActionClear;
		if (frame_msaa)
		{
			pass.colorAttachments[1].texture = msaa_glow;
			pass.colorAttachments[1].resolveTexture = frame_glow ? world_glow : nil;
			pass.colorAttachments[1].storeAction = frame_glow ? MTLStoreActionMultisampleResolve : MTLStoreActionDontCare;
		}
		else
		{
			pass.colorAttachments[1].texture = frame_glow ? world_glow : glow_scratch;
			pass.colorAttachments[1].storeAction = frame_glow ? MTLStoreActionStore : MTLStoreActionDontCare;
		}
		// Ambient shadows (Round 12) read the distance image after the pass,
		// and so does the distance shade
		frame_ao = DurandalGL::Active() && Durandal::Enabled(Durandal::kAmbientShadows) && ensure_ao(pixel_width, pixel_height);
		frame_distance_shade = (DurandalGL::Active() && Durandal::Available()) ? Durandal::Prefs().distance_shade / 100.0f : 0.0f;
		const bool keep_distance = frame_ao || frame_distance_shade > 0;
		pass.colorAttachments[2].texture = frame_msaa ? msaa_distance : world_distance;
		pass.colorAttachments[2].loadAction = MTLLoadActionClear;
		pass.colorAttachments[2].clearColor = MTLClearColorMake(kFarDistance, 0, 0, 0);
		if (frame_msaa)
		{
			pass.colorAttachments[2].resolveTexture = keep_distance ? world_distance : nil;
			pass.colorAttachments[2].storeAction = keep_distance ? MTLStoreActionMultisampleResolve : MTLStoreActionDontCare;
		}
		else
			pass.colorAttachments[2].storeAction = keep_distance ? MTLStoreActionStore : MTLStoreActionDontCare;
		pass.depthAttachment.loadAction = MTLLoadActionClear;
		pass.depthAttachment.clearDepth = 1.0;
		pass.depthAttachment.storeAction = MTLStoreActionDontCare;
		// Opened at the first draw (open_world_encoder)
		encoder = nil;
		timing::begin_frame(device);
		pending_pass = pass;
		pending_backdrop = backdrop;
	}
	current_texture = nil;
	current_sampler = nil;
	current_indices = nil;
	current_ramps = nil;
	in_world_pass = true;
	// The shaders read buffers 2 to 4 and texture 3 whatever the counts
	bound_light_count = 0;
	bound_caster_count = 0;
	bound_map = nil;
	current_volume = volume_none;
	return true;
}

// Pipeline, depth state, uniforms and textures for a draw with the bound
// material; false when nothing is bound or the encoder cannot open
static bool bind_material(const DrawState& state, const Uniforms& uniforms)
{
	const bool ramp = is_ramp_program(state.program);
	if (ramp ? !(current_indices && current_ramps) : !current_texture)
		return false;
	if (!open_world_encoder())
		return false;

	// Solid: unblended, and no texel can fail the alpha test
	static const bool never_solid = getenv("DURANDAL_NO_SOLID") != nullptr;	// development: compare
	const bool solid = !never_solid && state.blend == kBlendNone &&
		(uniforms.alpha_threshold < 0 || is_opaque(ramp ? current_indices : current_texture));
	[encoder setRenderPipelineState:(frame_msaa ? msaa_pipelines : pipelines)[state.program][state.blend][solid ? 0 : 1]];
	[encoder setDepthStencilState:depth_states[state.depth_test ? 1 : 0][state.depth_write ? 1 : 0]];
	[encoder setVertexBytes:&uniforms length:sizeof(Uniforms) atIndex:1];
	[encoder setFragmentBytes:&uniforms length:sizeof(Uniforms) atIndex:1];
	if (ramp)
	{
		[encoder setFragmentTexture:current_indices atIndex:1];
		[encoder setFragmentTexture:current_ramps atIndex:2];
	}
	else
	{
		[encoder setFragmentTexture:current_texture atIndex:0];
		id<MTLSamplerState> smp = current_sampler;
		if ((uniforms.filtering & 1) && (state.program == kWall || state.program == kSprite))
			smp = sampler_for(crisp_key(current_sampler_key, current_texture.mipmapLevelCount > 1));
		[encoder setFragmentSamplerState:smp atIndex:0];
		// HD art: the normal map (texture 5); wall shaders always have one
		if (!flat_bump)
		{
			MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
			d.usage = MTLTextureUsageShaderRead;
			d.storageMode = MTLStorageModeShared;
			flat_bump = [device newTextureWithDescriptor:d];
			const uint8_t flat[4] = { 128, 128, 255, 255 };
			[flat_bump replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:flat bytesPerRow:4];
		}
		[encoder setFragmentTexture:(uniforms.normal_map && current_bump) ? current_bump : flat_bump atIndex:5];
	}
	return true;
}

void Draw(const DrawState& state, const Uniforms& uniforms, const Vertex* triangles, int vertex_count)
{
	if (!in_world_pass || vertex_count < 3)
		return;
	static_assert(sizeof(Vertex) == 20, "Vertex must match PackedVertex");
	const size_t bytes = sizeof(Vertex) * vertex_count;
	if (bytes > 4096)
		return;	// larger than any Marathon polygon; setVertexBytes limit
	if (!bind_material(state, uniforms))
		return;
	// A shadow quad lies flat on the floor: its winding depends on where
	// the light is, so neither side is culled
	if (state.program == kShadow)
		[encoder setCullMode:MTLCullModeNone];
	[encoder setVertexBytes:triangles length:bytes atIndex:0];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertex_count];
	if (state.program == kShadow)
		[encoder setCullMode:MTLCullModeBack];
}

void DrawModel(const DrawState& state, const Uniforms& uniforms, const void* model,
			   const float* positions, const float* texcoords, int vertex_count,
			   const uint16_t* indices, int index_count, int sidedness)
{
	if (!in_world_pass || vertex_count < 3 || index_count < 3 || !positions || !indices)
		return;
	auto it = model_buffers.find(model);
	if (it == model_buffers.end() || it->second.vertex_count != vertex_count || it->second.index_count != index_count)
	{
		// Static models only: the vertices are kept as first seen
		ModelBuffers b;
		std::vector<Vertex> vertices(vertex_count);
		for (int i = 0; i < vertex_count; ++i)
		{
			vertices[i].x = positions[3 * i];
			vertices[i].y = positions[3 * i + 1];
			vertices[i].z = positions[3 * i + 2];
			vertices[i].u = texcoords ? texcoords[2 * i] : 0;
			vertices[i].v = texcoords ? texcoords[2 * i + 1] : 0;
		}
		b.vertices = [device newBufferWithBytes:vertices.data() length:sizeof(Vertex) * vertex_count options:MTLResourceStorageModeShared];
		b.indices = [device newBufferWithBytes:indices length:sizeof(uint16_t) * index_count options:MTLResourceStorageModeShared];
		b.vertex_count = vertex_count;
		b.index_count = index_count;
		it = model_buffers.insert_or_assign(model, b).first;
	}
	if (!it->second.vertices || !it->second.indices)
		return;
	if (!bind_material(state, uniforms))
		return;
	// The model's own sidedness (the world pass culls clockwise back faces)
	if (sidedness < 0)
		[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
	else if (sidedness == 0)
		[encoder setCullMode:MTLCullModeNone];
	[encoder setVertexBuffer:it->second.vertices offset:0 atIndex:0];
	[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:index_count indexType:MTLIndexTypeUInt16
					   indexBuffer:it->second.indices indexBufferOffset:0];
	if (sidedness < 0)
		[encoder setFrontFacingWinding:MTLWindingClockwise];
	else if (sidedness == 0)
		[encoder setCullMode:MTLCullModeBack];
}

void EndWorld(float gamma)
{
	if (!in_world_pass)
		return;
	@autoreleasepool {
		open_world_encoder();	// clears (and the backdrop) even with nothing drawn
		in_world_pass = false;
		[encoder endEncoding];
		encoder = nil;
		if (!DurandalGL::Active())
			timing::end_frame(command_buffer);	// the display ends it after its output pass (EndFrameTiming)

		if (DurandalGL::Active())
		{
			// Metal display: no wait; the world is drawn into the current
			// viewport of the frame being built
			const bool ao_now = frame_ao && !viewer_under_liquid;
			if (ao_now)
				run_ao();
			if (capture_requested)
			{
				id<MTLBuffer> readback = [device newBufferWithLength:size_t(target_width) * target_height * 4 options:MTLResourceStorageModeShared];
				id<MTLBlitCommandEncoder> blit = [command_buffer blitCommandEncoder];
				[blit copyFromTexture:world_color sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
						   sourceSize:MTLSizeMake(target_width, target_height, 1)
							 toBuffer:readback destinationOffset:0 destinationBytesPerRow:target_width * 4
					destinationBytesPerImage:size_t(target_width) * target_height * 4];
				[blit endEncoding];
				DurandalGL::FlushAndWait();
				capture_requested = false;
				const uint8_t* p = static_cast<const uint8_t*>(readback.contents);
				capture_pixels.assign(p, p + size_t(target_width) * target_height * 4);
				capture_width = target_width;
				capture_height = target_height;
				capture_ready = true;
			}
			if (frame_bloom)
				run_bloom();
			command_buffer = nil;
			current_texture = nil;
			DurandalGL::DrawWorldImage((__bridge void*)world_color, gamma, frame_glow_edr ? (__bridge void*)world_glow : nullptr,
									   frame_bloom ? (__bridge void*)bloom : nullptr,
									   ao_now ? (__bridge void*)ao_texture : nullptr,
									   (ao_now || frame_distance_shade > 0) ? (__bridge void*)world_distance : nullptr,
									   ao_now ? ao_far_reach : 0, ao_dev_view, frame_distance_shade);
			return;
		}

		MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
		pass.colorAttachments[0].texture = output_texture;
		pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		id<MTLRenderCommandEncoder> g = [command_buffer renderCommandEncoderWithDescriptor:pass];
		[g setRenderPipelineState:gamma_pipeline];
		[g setFragmentBytes:&gamma length:sizeof(float) atIndex:0];
		[g setFragmentTexture:world_color atIndex:0];
		[g drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
		[g endEncoding];

		id<MTLBuffer> readback = nil;
		if (capture_requested)
		{
			readback = [device newBufferWithLength:size_t(target_width) * target_height * 4 options:MTLResourceStorageModeShared];
			id<MTLBlitCommandEncoder> blit = [command_buffer blitCommandEncoder];
			[blit copyFromTexture:world_color sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
					   sourceSize:MTLSizeMake(target_width, target_height, 1)
						 toBuffer:readback destinationOffset:0 destinationBytesPerRow:target_width * 4
				destinationBytesPerImage:size_t(target_width) * target_height * 4];
			[blit endEncoding];
		}

		[command_buffer commit];
		// Bridge: OpenGL reads the IOSurface next, so wait for the GPU.
		[command_buffer waitUntilCompleted];
		command_buffer = nil;

		if (readback)
		{
			capture_requested = false;
			const uint8_t* p = static_cast<const uint8_t*>(readback.contents);
			capture_pixels.assign(p, p + size_t(target_width) * target_height * 4);
			capture_width = target_width;
			capture_height = target_height;
			capture_ready = true;
		}
	}
	current_texture = nil;
	current_bump = nil;
	draw_output_in_gl();
}

bool UseTexture(TextureState& state, int which)
{
	state.MetalLastUsed = which;
	const bool needs_load = !state.MetalTextures[which];
	if (which == TextureState::Bump)
	{
		current_bump = (__bridge id<MTLTexture>)state.MetalTextures[which];
		return needs_load;
	}
	current_texture = (__bridge id<MTLTexture>)state.MetalTextures[which];
	current_sampler_key = state.MetalSamplerKey[which];
	current_sampler = sampler_for(current_sampler_key);
	return needs_load;
}

// Creates the Metal texture for an image, with the sampler key the
// OpenGL settings imply (gl_near_filter/gl_far_filter as the
// GL_NEAREST/GL_LINEAR/..._MIPMAP_... values; anisotropy_level the
// preference, 0-16). Nil when it cannot.
static id<MTLTexture> create_texture(const ImageDescriptor* image, short texture_type, bool landscape_vert_repeat,
									 int gl_near_filter, int gl_far_filter, float anisotropy_level, bool srgb,
									 uint32_t& sampler_key)
{
	if (!image)
		return nil;
	const int format = image->GetFormat();
	if (format != ImageDescriptor::RGBA8 && format != ImageDescriptor::DXTC1 &&
		format != ImageDescriptor::DXTC3 && format != ImageDescriptor::DXTC5 && format != ImageDescriptor::BC7)
		return nil;

	// HD art: a block-compressed image (DDS, or the texture cache's BC7)
	// uploads as BC1/BC2/BC3/BC7 with the mip levels it provides (a chain
	// missing its last level is fine). Metal cannot generate mips for
	// those formats, so one without a chain is decompressed first and
	// treated as RGBA
	std::unique_ptr<ImageDescriptor> decompressed;
	if (format != ImageDescriptor::RGBA8 && image->GetMipMapCount() <= 1)
	{
		decompressed.reset(new ImageDescriptor(*image));
		if (!decompressed->MakeRGBA())
			return nil;
		image = decompressed.get();
	}
	const bool compressed = image->GetFormat() != ImageDescriptor::RGBA8;
	MTLPixelFormat pixel_format;
	size_t block_bytes = 0;
	switch (image->GetFormat())
	{
		case ImageDescriptor::DXTC1: pixel_format = srgb ? MTLPixelFormatBC1_RGBA_sRGB : MTLPixelFormatBC1_RGBA; block_bytes = 8; break;
		case ImageDescriptor::DXTC3: pixel_format = srgb ? MTLPixelFormatBC2_RGBA_sRGB : MTLPixelFormatBC2_RGBA; block_bytes = 16; break;
		case ImageDescriptor::DXTC5: pixel_format = srgb ? MTLPixelFormatBC3_RGBA_sRGB : MTLPixelFormatBC3_RGBA; block_bytes = 16; break;
		case ImageDescriptor::BC7: pixel_format = srgb ? MTLPixelFormatBC7_RGBAUnorm_sRGB : MTLPixelFormatBC7_RGBAUnorm; block_bytes = 16; break;
		default: pixel_format = srgb ? MTLPixelFormatRGBA8Unorm_sRGB : MTLPixelFormatRGBA8Unorm; break;
	}

	// Sampler from the GL settings the OpenGL path would use
	uint32_t mag = (gl_near_filter == GL_LINEAR) ? 1 : 0;
	uint32_t min = 0, mip = 0;
	switch (gl_far_filter)
	{
		case GL_LINEAR: min = 1; break;
		case GL_NEAREST_MIPMAP_NEAREST: min = 0; mip = 1; break;
		case GL_LINEAR_MIPMAP_NEAREST: min = 1; mip = 1; break;
		case GL_NEAREST_MIPMAP_LINEAR: min = 0; mip = 2; break;
		case GL_LINEAR_MIPMAP_LINEAR: min = 1; mip = 2; break;
		default: break;
	}
	uint32_t wrap_s = 0, wrap_t = 0, aniso = 1;
	switch (texture_type)
	{
		case OGL_Txtr_Wall:
			if (anisotropy_level > 0)
				aniso = std::min<uint32_t>(16, 1 + uint32_t(((anisotropy_level - 1) / 15.f) * 15.f + 0.5f));
			break;
		case OGL_Txtr_Landscape:
			wrap_t = landscape_vert_repeat ? 0 : 1;
			break;
		default:
			wrap_s = wrap_t = 2;
			break;
	}

	const int w = image->GetWidth(), h = image->GetHeight();
	// Walls and sprites always get mipmaps, so crisp filtering (L4) can
	// switch on live; a non-mipmapped sampler only reads level 0
	const bool mipmapped = mip != 0 || texture_type != OGL_Txtr_Landscape;
	const int provided = image->GetMipMapCount();
	MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixel_format
																			  width:w height:h mipmapped:mipmapped];
	if (compressed && mipmapped)
		d.mipmapLevelCount = std::max(1, provided);	// only the file's levels exist
	d.usage = MTLTextureUsageShaderRead;
	d.storageMode = MTLStorageModeShared;
	id<MTLTexture> t = [device newTextureWithDescriptor:d];
	if (!t)
		return nil;

	{
		const uint8_t* p = reinterpret_cast<const uint8_t*>(image->GetMipMapPtr(0));
		bool opaque = compressed ? image->Opaque : p != nullptr;	// the texture cache records it
		for (size_t i = 0; opaque && !compressed && i < size_t(w) * h; ++i)
			opaque = p[4 * i + 3] == 255;
		opaque_textures[(__bridge const void*)t] = opaque;
	}
	const int levels = mipmapped ? (int)t.mipmapLevelCount : 1;
	const int upload = mipmapped ? std::min(levels, std::max(provided, 1)) : 1;
	for (int level = 0; level < upload; ++level)
	{
		const int lw = std::max(1, w >> level), lh = std::max(1, h >> level);
		const void* pixels = image->GetMipMapPtr(level);
		if (!pixels)
			break;
		const size_t row = compressed ? size_t((lw + 3) / 4) * block_bytes : size_t(lw) * 4;
		[t replaceRegion:MTLRegionMake2D(0, 0, lw, lh) mipmapLevel:level withBytes:pixels bytesPerRow:row];
	}
	if (!compressed && mipmapped && upload < levels)
	{
		// As gluBuild2DMipmaps: generate the rest on the GPU. Committed
		// before the world command buffer, so it runs first.
		id<MTLCommandBuffer> cb = [queue commandBuffer];
		id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
		[blit generateMipmapsForTexture:t];
		[blit endEncoding];
		[cb commit];
	}
	sampler_key = mag | (min << 1) | (mip << 2) | (wrap_s << 4) | (wrap_t << 6) | (aniso << 8);
	return t;
}

void PlaceTexture(TextureState& state, int which, const ImageDescriptor* image, short texture_type,
				  bool landscape_vert_repeat, int gl_near_filter, int gl_far_filter, float anisotropy_level, bool srgb)
{
	if (!init() || !image)
		return;
	@autoreleasepool {
		uint32_t key = 0;
		id<MTLTexture> t = create_texture(image, texture_type, landscape_vert_repeat, gl_near_filter, gl_far_filter,
										  anisotropy_level, srgb, key);
		if (!t)
			return;
		if (state.MetalTextures[which])
			CFRelease(state.MetalTextures[which]);
		state.MetalTextures[which] = (void*)CFBridgingRetain(t);
		state.MetalSamplerKey[which] = key;

		if (which == TextureState::Bump)
		{
			current_bump = t;	// HD art: sampled with the colour's sampler
			return;
		}
		current_texture = t;
		current_sampler_key = state.MetalSamplerKey[which];
		current_sampler = sampler_for(current_sampler_key);
	}
}

// 3D pickups: skins are kept by their OGL_SkinData (per image), models by
// their OGL_ModelData; both live until the models are unloaded
bool PlaceModelSkin(const void* skin, int which, const ImageDescriptor* image)
{
	if (!init() || !image || !image->IsPresent())
		return false;
	const void* key = static_cast<const char*>(skin) + which;
	auto it = model_skins.find(key);
	if (it == model_skins.end())
	{
		@autoreleasepool {
			uint32_t sampler = 0;
			id<MTLTexture> t = create_texture(image, OGL_Txtr_Inhabitant, false, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, 0, false, sampler);
			it = model_skins.emplace(key, t).first;
			model_skin_samplers[key] = sampler;
		}
	}
	if (!it->second)
		return false;
	current_texture = it->second;
	current_sampler_key = model_skin_samplers[key];
	current_sampler = sampler_for(current_sampler_key);
	return true;
}

void ReleaseModels()
{
	for (auto& it : model_skins)
		if (it.second)
			opaque_textures.erase((__bridge const void*)it.second);
	model_skins.clear();
	model_skin_samplers.clear();
	model_buffers.clear();
}

// Mip level from the one above: per 2x2 block, the source index whose
// colour is nearest the block's opacity-weighted average colour (as Quake
// built its 8-bit mipmaps, but choosing among the block's own colours so
// every pixel stays on an authored ramp); opacity is the block's average.
// Mip level from the one above: per 2x2 block, the source index whose
// colour is nearest the block's opacity-weighted average colour (as Quake
// built its 8-bit mipmaps, but choosing among the block's own colours so
// every pixel stays on an authored ramp); opacity and glow are the block's
// averages. Texels are RGBA8: index, opacity, glow, unused.
static void downsample_indices(const std::vector<uint8_t>& src, int sw, int sh, std::vector<uint8_t>& dst, int dw, int dh,
							   const uint32_t* colours)
{
	dst.assign(size_t(dw) * dh * 4, 0);
	for (int y = 0; y < dh; ++y)
		for (int x = 0; x < dw; ++x)
		{
			int idx[4], alpha[4], glow = 0, height = 0, n = 0;
			for (int j = 0; j < 2; ++j)
				for (int i = 0; i < 2; ++i)
				{
					const int sx = std::min(sw - 1, 2 * x + i), sy = std::min(sh - 1, 2 * y + j);
					const uint8_t* p = &src[(size_t(sy) * sw + sx) * 4];
					idx[n] = p[0];
					alpha[n] = p[1];
					glow += p[2];
					height += p[3];
					++n;
				}
			float avg[3] = {0, 0, 0};
			int total = 0;
			for (int k = 0; k < 4; ++k)
			{
				const uint8_t* c = reinterpret_cast<const uint8_t*>(colours + idx[k]);
				for (int q = 0; q < 3; ++q)
					avg[q] += c[q] * alpha[k];
				total += alpha[k];
			}
			int best = 0;
			if (total > 0)
			{
				float best_d = 1e30f;
				for (int k = 0; k < 4; ++k)
				{
					if (!alpha[k])
						continue;
					const uint8_t* c = reinterpret_cast<const uint8_t*>(colours + idx[k]);
					float d = 0;
					for (int q = 0; q < 3; ++q)
					{
						const float e = c[q] - avg[q] / total;
						d += e * e;
					}
					if (d < best_d)
					{
						best_d = d;
						best = k;
					}
				}
			}
			uint8_t* o = &dst[(size_t(y) * dw + x) * 4];
			o[0] = uint8_t(idx[best]);
			o[1] = uint8_t((total + 2) / 4);
			o[2] = uint8_t((glow + 2) / 4);
			o[3] = uint8_t((height + 2) / 4);	// relief (M1): 0 stays 0
		}
}

void PlaceIndexTexture(TextureState& state, const uint8_t* texels, int w, int h, const uint32_t* colours)
{
	if (!init() || w <= 0 || h <= 0)
		return;
	@autoreleasepool {
		MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Uint
																				  width:w height:h mipmapped:YES];
		d.usage = MTLTextureUsageShaderRead;
		d.storageMode = MTLStorageModeShared;
		id<MTLTexture> t = [device newTextureWithDescriptor:d];
		if (!t)
			return;
		bool opaque = true;
		for (size_t i = 0; opaque && i < size_t(w) * h; ++i)
			opaque = texels[4 * i + 1] == 255;
		opaque_textures[(__bridge const void*)t] = opaque;
		std::vector<uint8_t> level(texels, texels + size_t(w) * h * 4), next;
		int lw = w, lh = h;
		for (NSUInteger k = 0; k < t.mipmapLevelCount; ++k)
		{
			[t replaceRegion:MTLRegionMake2D(0, 0, lw, lh) mipmapLevel:k withBytes:level.data() bytesPerRow:lw * 4];
			const int nw = std::max(1, lw / 2), nh = std::max(1, lh / 2);
			if (k + 1 < t.mipmapLevelCount)
			{
				downsample_indices(level, lw, lh, next, nw, nh, colours);
				level.swap(next);
			}
			lw = nw;
			lh = nh;
		}
		if (state.MetalIndexTexture)
			CFRelease(state.MetalIndexTexture);
		state.MetalIndexTexture = (void*)CFBridgingRetain(t);
	}
}

bool BindRamps(TextureState& state, short collection, short clut)
{
	current_indices = nil;
	current_ramps = nil;
	if (!state.MetalIndexTexture)
		return false;
	if (ramp_generation != DurandalShading::Generation())
	{
		ramp_textures.clear();
		ramp_generation = DurandalShading::Generation();
	}
	const uint32_t key = (uint32_t(uint16_t(collection)) << 16) | uint16_t(clut);
	auto it = ramp_textures.find(key);
	if (it == ramp_textures.end())
	{
		const DurandalShading::Ramps* r = DurandalShading::Get(collection, clut);
		id<MTLTexture> t = nil;
		if (r)
		{
			std::vector<uint8_t> shades;
			bake_shades(*r, shades);
			MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																					  width:256 height:kShadeRows mipmapped:NO];
			d.usage = MTLTextureUsageShaderRead;
			d.storageMode = MTLStorageModeShared;
			t = [device newTextureWithDescriptor:d];
			[t replaceRegion:MTLRegionMake2D(0, 0, 256, kShadeRows) mipmapLevel:0 withBytes:shades.data() bytesPerRow:256 * 4];
		}
		it = ramp_textures.emplace(key, t).first;
	}
	if (!it->second)
		return false;
	current_indices = (__bridge id<MTLTexture>)state.MetalIndexTexture;
	current_ramps = it->second;
	return true;
}

void ReleaseTextures(TextureState& state)
{
	if (state.MetalIndexTexture)
	{
		CFRelease(state.MetalIndexTexture);
		state.MetalIndexTexture = nullptr;
	}
	for (int i = 0; i < TextureState::NUMBER_OF_TEXTURES; ++i)
	{
		if (state.MetalTextures[i])
		{
			CFRelease(state.MetalTextures[i]);
			state.MetalTextures[i] = nullptr;
		}
		state.MetalSamplerKey[i] = 0;
	}
}

}
