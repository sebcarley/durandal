#ifndef DURANDAL_METAL_H
#define DURANDAL_METAL_H
/*
	DurandalMetal.h — Durandal project

	Core of the Metal world renderer (roadmap F3): device, shader pipelines,
	textures, and the per-frame world pass.

	Milestone 2a ("bridge"): the world is drawn with Metal into an
	IOSurface-backed texture, which the existing OpenGL frame then shows in
	the world view before drawing the HUD, map, fades and menus on top.
	This lets the Metal world be compared against OpenGL and switched live,
	before the 2D layer moves to Metal.

	Plain C++ interface; the Objective-C++ lives in DurandalMetal.mm.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include <simd/simd.h>
#include <cstdint>
#include <vector>

class ImageDescriptor;
struct TextureState;

namespace DurandalMetal {

// Must match struct Uniforms in DurandalMetalShaders.h.
struct Uniforms {
	simd_float4x4 projection;
	simd_float4x4 modelview;
	simd_float4x4 texture_matrix;
	simd_float4x4 model;		// 3D pickups: model -> world; identity for the world's own polygons
	simd_float4 clip_planes[3];
	simd_float4 color;
	simd_float4 fog_color;
	simd_float4 normal;
	simd_float4 tangent;
	float depth_offset;
	float pulsate;
	float wobble;
	float glow;
	float flare;
	float self_luminosity;
	float visibility;
	float transfer_fade_out;
	float time;
	float static_block;
	float fog_mode;
	float fog_start;
	float fog_end;
	float fog_density;
	float fog_mix;
	float scalex;
	float scaley;
	float offsetx;
	float offsety;
	float yaw;
	float pitch;
	float alpha_threshold;
	uint32_t clip_mask;
	uint32_t shading_style;		// 8-bit shading: 0 banded, 1 smooth
	uint32_t texel_lighting;	// 1: light each texel at its centre (L2)
	uint32_t filtering;			// bit 0: crisp texture filtering (L4)
	float emissive;				// glow (E1): the object's minimum light, 0-1
	uint32_t write_glow;		// 1: write the glow image
	uint32_t sky;				// HDR sky (V2): bit 0 cylindrical projection, bit 1 sky glow
	uint32_t light_count;		// dynamic lights (E2) set by SetLights
	simd_float4 camera;			// viewer position, world units
	int32_t polygon;			// the map polygon being drawn (light shadows, E3)
	uint32_t shadows;			// 1: lights are blocked by the map (SetMap)
	float caustics;				// liquids (W1): caustic strength on surfaces below
	float media_height;			//   the liquid's surface height
	simd_float4 liquid;			//   liquid surfaces: type + 1, murk distance, opacity, waves
	simd_float4 liquid_colour;	//   absorption per channel, w: surface glow
	uint32_t caster_count;		// contact shadows: casters set by SetCasters
	simd_float4 volume_screen;	// volumetric fog (V1): 1/width, 1/height (pixels),
								//   slices per log distance, nearest slice distance
	uint32_t volume;			//   1: apply the fog volume (texture 3)
	int32_t patch;				// light redistribution (E4): this surface's patch, -1 none
	uint32_t gi;				//   1: apply it
	simd_float4 gi_range;		//   x: lowest ratio below 1, y: highest above, z: colour bleed, w: its cap
	float relief;				// surface relief (M1): strength, 0 off
	float roll;					// True Look (C1): the camera's roll, radians (Sidestep Sway)
	uint32_t normal_map;		// HD art: a pack's normal map is bound (texture 5): 1 y up the image, 2 y down
	float bloom_scale;			// HD art: the pack's bloom scale and shift for this image
	float bloom_shift;			//   (normal_bloom_* for the colour image, glow_bloom_* for its glow)
	float normal_gain;			// HD art: the normal map's strength
	float glow_gain;			// HD art: glow boost on a replacement sprite's bright pixels (1: none)
	float distance_mode;		// Round 12: the distance image: 1 the fragment's distance where its alpha is over a half, 0 never, -1 and 2 far as the sky (no ambient shadow or distance shade, occludes nothing): -1 the weapon in hand (also fogged as right at the face), 2 landscape surfaces
	simd_float4 viewer_light;	// weapon lighting: the dynamic lights at the viewer, for the weapon in hand (rgb the tint, a the amount; 0 none)
	simd_int4 figure_patches;	// Bounced Light (R4): a sprite's floor and ceiling patches (x, y), -1 none;
								//   z, w: a figure's own position (its card casts no shadow on itself), 1e5 none
	simd_int4 rampant;			// Rampant: x traced shadows (R1), y polygons in the occluder lists
};

// Dynamic lights (E2), world units. Must match struct Light in the shaders.
struct Light {
	simd_float4 position_radius;
	simd_float4 colour_strength;
	simd_int4 info;				// x: the map polygon the light is in
};
constexpr int kMaximumLights = 16;

// Contact shadows, world units. Must match struct Caster in the shaders.
struct Caster {
	simd_float4 position_radius;	// x, y, the floor's height under it, radius
	simd_float4 info;				// x: strength, 0-1
};
constexpr int kMaximumCasters = 32;

// Traced shadows (R1, Rampant), world units. Must match struct Occluder in
// the shaders. A figure's card, turned to face each ray that meets it,
// with its frame's silhouette (a slice of the mask array, SetMask).
struct Occluder {
	simd_float4 position;		// the object's origin
	simd_float4 extent;			// left, right, bottom, top from the origin, along the card
	simd_float4 mask;			// the bitmap's share of its slice across and down, mask texels per world unit, 0
	simd_int4 info;				// x: mask slice, y: mirrored, z, w: 0
};
constexpr int kMaximumOccluders = 128;
constexpr int kMaskSize = 256;		// a slice of the mask array, texels across
constexpr int kMaskSlices = 512;

struct Vertex {
	float x, y, z;
	float u, v;
};

enum Program {
	kWall,
	kWallInfravision,
	kSprite,
	kSpriteInfravision,
	kInvincible,
	kInvisible,
	kLandscape,
	kLandscapeInfravision,
	kLandscapeSphere,
	kLandscapeSphereInfravision,
	kWallRamp,		// 8-bit shading (L1): colour indices through the ramps
	kSpriteRamp,
	kLiquid,		// liquid surfaces (W1): read what is below, draw over it
	kShadow,		// character shadows (Round 12): a sprite's silhouette darkening the floor
	kNumberOfPrograms
};

// Matches OGL_BlendType_* values (OGL_Setup.h)
enum Blend {
	kBlendCrossfade = 0,
	kBlendAdd = 1,
	kBlendCrossfadePremultiplied = 2,
	kBlendAddPremultiplied = 3,
	kBlendNone = 4
};

struct DrawState {
	Program program = kWall;
	Blend blend = kBlendNone;
	bool depth_test = true;
	bool depth_write = true;
};

// True when the Metal world renderer should draw the world this frame:
// the Durandal "Metal renderer" switch is on and Metal initialised.
bool WorldRendererActive();

// Dynamic lights (E2) for the rest of this world pass (at most
// kMaximumLights); draws use them when their uniforms' light_count says so.
void SetLights(const Light* list, int count);
// Light shadows (E3): the map for this world pass (see the shaders'
// light_reaches for the layout: 9 float4 per polygon).
void SetMap(const simd_float4* map, int polygon_count);
// Contact shadows for the rest of this world pass (at most kMaximumCasters).
void SetCasters(const Caster* list, int count);

// Light redistribution (E4): the surface cache. Each floor, ceiling and
// wall of the map is a patch of lumels in one atlas. Must match struct
// Patch in the shaders.
struct Patch {
	simd_float4 origin;			// the corner of lumel (0, 0)
	simd_float4 u;				// world -> lumels across the patch (axis / lumel size)
	simd_float4 v;				// world -> lumels up the patch
	simd_float4 du;				// one lumel across, in the world
	simd_float4 dv;				// one lumel up, in the world
	simd_float4 normal;			// facing into its polygon
	simd_int4 rect;				// atlas x, y, width, height (lumels)
	simd_int4 info;				// kind (0 floor, 1 ceiling, 2 wall), polygon, edge, group
};
// One 8x8 block of a patch's lumels to bake this frame
struct BakeTile {
	simd_int4 tile;				// patch, first lumel across, first lumel up, 0
	simd_float4 blend;			// x: weight of this frame's samples against what is there
};
constexpr int kBakeTile = 8;

// The layout of the surface cache (a new level): the patches, the atlas
// size, and the groups of patches that share an average (adjacent
// coplanar surfaces with the same texture and light: group_start has one
// entry per group plus one, indexing group_members). Forgets what was
// baked.
void SetRadianceLayout(const std::vector<Patch>& patches, int atlas_width, int atlas_height,
					   const std::vector<int>& group_start, const std::vector<int>& group_members);
// Bakes `tiles` with `rays` rays per lumel (map set by SetMap; `surfaces`:
// what each surface gives off, 10 float4 per polygon: floor, ceiling, then
// each edge's wall; w 1 = sky), then updates the averages of the patches
// in `baked`. Call before the first draw. seed: changes every frame.
// bounce (Bounced Light, R4): a ray sees the surface it hits as it is
// drawn, redistributed within `range` (the uniforms' gi_range), read from
// a copy of the atlas taken first.
bool RunRadiance(const std::vector<BakeTile>& tiles, const std::vector<int>& baked,
				 const std::vector<simd_float4>& surfaces, int rays, uint32_t seed,
				 bool bounce, simd_float4 range);
// Whether the surface cache exists (the draws may use patches)
bool RadianceReady();

// Volumetric fog (V1). Must match struct VolumeParams in the shaders.
// Densities are per world unit (1/1024 of WORLD_ONE).
struct VolumeParams {
	simd_float4x4 inverse;		// (projection x modelview)^-1: clip -> world
	simd_float4 camera;			// viewer position
	simd_float4 air;			// in-scattered colour of the haze (x the room's light), w: density
	simd_float4 murk;			// the same under a liquid the viewer is in
	simd_uint4 grid;			// columns, rows, slices, dynamic light count
	int32_t polygon;			// the viewer's polygon
	uint32_t viewer_under;		// 1: the viewer is under a liquid
	uint32_t shadows;			// 1: dynamic lights are blocked by the map
	float time;					// seconds
	float nearest;				// the first slice's distance (set by RunVolume)
	float slice_scale;			// slices per log distance (set by RunVolume)
	float dust;					// density variation, 0-1
	float light_scale;			// how brightly dynamic lights show in the haze
	float mist;					// extra density near the floor, x the base
	float pad[3];
};
constexpr int kVolumeSlices = 64;
constexpr int kVolumeCell = 8;	// pixels per column, each way

// Volumetric fog: builds this frame's fog volume from the lights and map
// already set (SetLights, SetMap) and has the world's draws apply it
// (their uniforms' volume and volume_screen). Call before the first draw.
// Returns false when it cannot run; otherwise sets `screen` for the
// uniforms' volume_screen.
bool RunVolume(VolumeParams params, simd_float4& screen);

// Glow (E1): whether this frame's world pass writes the glow image (set
// by BeginWorld from the Glow setting and whether HDR output is showing).
bool GlowActive();

// Ambient shadows (Round 12): the view this frame, for reconstructing
// positions from the distance image (world -> clip, clip -> world, the
// viewer). Call before the first draw.
void SetView(simd_float4x4 view_projection, simd_float4x4 inverse, simd_float4 camera);
// Ambient shadows are skipped while the viewer is under a liquid (the murk
// already gives the depth, and creases came out as black mould)
void SetViewerUnderLiquid(bool under);

// True between BeginWorld() and EndWorld(). Texture calls made during
// this window go to Metal; all others (the OpenGL HUD etc.) go to OpenGL.
bool InWorldPass();

// Frame. pixel_width/height: world view size in drawable pixels.
bool BeginWorld(int pixel_width, int pixel_height, bool keep_previous_frame, simd_float4 clear_color);
void Draw(const DrawState& state, const Uniforms& uniforms, const Vertex* triangles, int vertex_count);
// Finishes the world, applies gamma, and draws the result into the
// current OpenGL viewport.
void EndWorld(float gamma);

// Textures (called from OGL_Textures.cpp while InWorldPass()).
// Binds the texture in `state` slot `which` for the next Draw(); returns
// true if it has not been created yet and needs PlaceTexture(). The Bump
// slot (HD art: a pack's normal map) binds beside the colour texture.
bool UseTexture(TextureState& state, int which);
// gl_near_filter/gl_far_filter are the GL_NEAREST/GL_LINEAR/..._MIPMAP_...
// values the OpenGL path would use; anisotropy_level is the preference (0-16).
void PlaceTexture(TextureState& state, int which, const ImageDescriptor* image, short texture_type,
				  bool landscape_vert_repeat, int gl_near_filter, int gl_far_filter, float anisotropy_level, bool srgb);
void ReleaseTextures(TextureState& state);

// 3D pickups (HD art): a model drawn in place of a sprite. Its vertices
// (positions in engine units, texture coordinates, both as the engine's
// Model3D holds them) and indices are kept per `model` key until
// ReleaseModels(); `uniforms.model` places it. sidedness as
// OGL_ModelData::Sidedness (+ clockwise faces visible, - counterclockwise,
// 0 both). Draws with the skin bound by PlaceModelSkin.
void DrawModel(const DrawState& state, const Uniforms& uniforms, const void* model,
			   const float* positions, const float* texcoords, int vertex_count,
			   const uint16_t* indices, int index_count, int sidedness);
// The texture for skin `skin` (an OGL_SkinData) image `which`, created from
// `image` on first use, bound for the next DrawModel; false without one.
bool PlaceModelSkin(const void* skin, int which, const ImageDescriptor* image);
// Forgets every model's buffers and skins (the models are unloaded).
void ReleaseModels();

// 8-bit shading (L1). PlaceIndexTexture stores a texture's colour indices
// (R), opacities (G) and glow (B, E1), laid out as its RGBA image, with
// mip levels. BindRamps binds that
// and the colour table's ramps for the next ramp-program Draw(); false if
// either is missing (draw in true colour instead).
// `colours` (RGBA8 per index, at full brightness) picks each mip level's
// indices.
void PlaceIndexTexture(TextureState& state, const uint8_t* texels, int width, int height, const uint32_t* colours);
bool BindRamps(TextureState& state, short collection, short clut);

// Development (parity check, see DurandalBenchmark): force the world
// renderer on (1) or off (0) regardless of the setting; -1 follows it.
void ForceWorldRenderer(int mode);
// Capture the next world image, before gamma, from whichever renderer
// draws it. Rows are top-down RGBA8.
void RequestCapture();
bool CaptureRequested();
void CaptureGLFramebuffer(int pixel_width, int pixel_height);	// Rasterizer_Shader::End
bool TakeCapture(std::vector<uint8_t>& rgba, int& width, int& height);

// True when the whole frame is presented through Metal (DurandalGL display).
bool DisplayActive();

// Traced shadows (R1): this frame's figures, and per polygon the ones a ray
// crossing it may meet (polygons: first index into `indices`, count). Call
// before the first draw; count 0 clears them.
void SetOccluders(const Occluder* list, int count, const simd_int2* polygons, int polygon_count,
				  const int* indices, int index_count);
// A silhouette into slice `slice` of the mask array: kMaskSize x kMaskSize
// opacity (0 clear, 255 solid) at level 0, then each mip level down to 1x1.
bool SetMask(int slice, const std::vector<std::vector<uint8_t>>& levels);

// Development (DURANDAL_GPU_TIMING): the display's own passes join a world
// frame's stage timings. `pass` is an MTLRenderPassDescriptor, `command_buffer`
// the frame's; the display ends the frame's timing after its output pass.
// No effect without timing or outside a frame with a world.
enum TimedDisplayPass { kTimedCanvas, kTimedOutput };
void TimeDisplayPass(void* pass, TimedDisplayPass which);
void EndFrameTiming(void* command_buffer);

}

#endif
