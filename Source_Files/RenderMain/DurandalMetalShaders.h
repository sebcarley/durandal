/*
	DurandalMetalShaders.h — Durandal project

	Metal Shading Language source for the Metal world renderer, compiled at
	start-up. These are line-for-line ports of the GLSL in Shaders/ (wall,
	sprite, landscape, landscape_sphere, invincible, invisible and the
	infravision variants), so the Metal renderer matches the OpenGL shader
	renderer. Positions are computed in OpenGL clip space first so depth
	nudges and the headlight falloff ("classicDepth") are identical; depth
	is converted to Metal's [0,1] range only at output.

	The Uniforms struct must match DurandalMetal::Uniforms in DurandalMetal.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#ifndef DURANDAL_METAL_SHADERS_H
#define DURANDAL_METAL_SHADERS_H

static const char* kDurandalMetalShaderSource = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct Uniforms {
	float4x4 projection;		// OpenGL-convention projection
	float4x4 modelview;			// world -> eye (OpenGL eye space)
	float4x4 texture_matrix;
	float4x4 model;				// 3D pickups: model -> world; identity for the world's own polygons
	float4 clip_planes[3];		// in the vertex's own space: left, right, liquid surface
	float4 color;				// glColor equivalent
	float4 fog_color;
	float4 normal;				// xyz; zero when there is no TBN
	float4 tangent;				// xyz, w = bitangent sign
	float depth_offset;			// walls only (wall.vert "depth")
	float pulsate;
	float wobble;
	float glow;
	float flare;
	float self_luminosity;
	float visibility;
	float transfer_fade_out;
	float time;
	float static_block;
	float fog_mode;				// -1 none, 0 linear, 1 exp, 2 exp2
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
	float alpha_threshold;		// discard when alpha <= this; < 0 = no test
	uint clip_mask;
	uint shading_style;			// 8-bit shading: 0 banded, 1 smooth
	uint texel_lighting;		// 1: light each texel at its centre (L2)
	uint filtering;				// bit 0: crisp texture filtering (L4)
	float emissive;				// glow (E1): this object's minimum light, 0-1
	uint write_glow;			// 1: write the glow image
	uint sky;					// HDR sky (V2): bit 0 cylindrical projection, bit 1 sky glow, bit 2 no vertical repeat
	uint light_count;			// dynamic lights (E2) in buffer 2
	float4 camera;				// viewer position, world units
	int polygon;				// the map polygon being drawn, for light shadows (E3)
	uint shadows;				// 1: lights are blocked by the map (buffer 3)
	float caustics;				// liquids (W1): caustic strength on surfaces below
	float media_height;			//   the liquid's surface height, world units
	float4 liquid;				//   liquid surfaces: type + 1 (0: not a liquid), murk distance, opacity, waves
	float4 liquid_colour;		//   per-channel absorption (murk tint), w: surface glow
	uint caster_count;			// contact shadows: casters in buffer 4
	float4 volume_screen;		// volumetric fog (V1): 1/width, 1/height, slices per log distance, first slice
	uint volume;				//   1: apply the fog volume (texture 3)
	int patch;					// light redistribution (E4): this surface's patch, -1 none
	uint gi;					//   1: apply it
	float4 gi_range;			//   x: lowest ratio below 1, y: highest above, z: colour bleed, w: its cap
	float relief;				// surface relief (M1): strength, 0 off
	float roll;					// True Look (C1): the camera's roll, radians (Sidestep Sway)
	uint normal_map;			// HD art: a pack's normal map is bound (texture 5): 1 y up the image, 2 y down
	float bloom_scale;			// HD art: the pack's bloom scale and shift for this image
	float bloom_shift;
	float normal_gain;			// HD art: the normal map's strength
	float glow_gain;			// HD art: glow boost on a replacement sprite's bright pixels (1: none)
	float distance_mode;		// Round 12: the distance image: 1 the fragment's distance where its alpha is over a half, 0 never, -1 and 2 far as the sky (no ambient shadow or distance shade, occludes nothing): -1 the weapon in hand (also fogged as right at the face), 2 landscape surfaces
	float4 viewer_light;		// weapon lighting: the dynamic lights at the viewer, for the weapon in hand (rgb the tint, a the amount; 0 none)
	int4 figure_patches;		// Bounced Light (R4): a sprite's floor and ceiling patches (x, y), -1 none;
								//   z, w: a figure's own position (its card casts no shadow on itself)
	int4 rampant;				// Rampant: x traced shadows (R1), y polygons in the occluder lists,
								//   z reflecting liquids (R2), w exact distances (R3, below)
	uint4 culling;				// x the lights, y the casters that can reach this draw (bits)
};

// Light redistribution (E4): one surface's lumels in the surface cache
struct Patch {
	float4 origin;
	float4 u;
	float4 v;
	float4 du;
	float4 dv;
	float4 normal;
	int4 rect;
	int4 info;
};

// Dynamic lights (E2), world units: position and radius; colour and
// strength; x: the map polygon the light is in, y: its size (traced shadows),
// z: 1 if it casts figures' shadows (the nearest few)
struct Light {
	float4 position_radius;
	float4 colour_strength;
	int4 info;
};

// Contact shadows, world units: x, y, the floor's height under the
// object, radius; x: strength
struct Caster {
	float4 position_radius;
	float4 info;
};

// Light shadows (E3): the map, 9 float4 per polygon: (vertex count, floor,
// ceiling, 0), then per edge (x, y of its first vertex, the polygon across
// it or -1, 0). A light reaches a point if a straight line between them
// passes only through openings: walking the polygons it crosses, as
// Marathon's own line-of-sight tests do, so 5D space works.
static bool light_reaches(device const float4* map, int poly, float3 from, float3 to, int light_poly)
{
	const float2 a = from.xy, d = to.xy - from.xy;
	float t_prev = 0.0;
	for (int step = 0; step < 24; ++step) {
		if (poly == light_poly)
			return true;
		if (poly < 0)
			return false;
		const float4 h = map[poly * 9];
		const int n = int(h.x);
		float best = 2.0;
		int next = -1;
		for (int i = 0; i < n; ++i) {
			const float4 e0 = map[poly * 9 + 1 + i];
			const float4 e1 = map[poly * 9 + 1 + (i + 1 == n ? 0 : i + 1)];
			const float2 e = e1.xy - e0.xy;
			const float den = d.x * e.y - d.y * e.x;
			if (abs(den) < 1e-6)
				continue;
			const float2 w = e0.xy - a;
			const float t = (w.x * e.y - w.y * e.x) / den;
			const float s = (w.x * d.y - w.y * d.x) / den;
			if (t > t_prev + 1e-4 && t < best && s >= -1e-3 && s <= 1.0 + 1e-3) {
				best = t;
				next = int(e0.z);
			}
		}
		if (best > 1.0)
			return true;	// the light is within this polygon's reach
		if (next < 0)
			return false;	// a solid wall
		const float z = mix(from.z, to.z, best);
		const float4 hn = map[next * 9];
		if (z < max(h.y, hn.y) - 1.0 || z > min(h.z, hn.z) + 1.0)
			return false;	// a step, ledge or lower ceiling in the way
		poly = next;
		t_prev = best;
	}
	return true;
}

// Traced shadows (R1, Rampant): the figures standing in the rooms, as
// cards that turn to face each ray, with their frame's silhouette (a slice
// of the mask array, mip levels for soft edges). See DurandalOccluders.h.
struct Occluder {
	float4 position;	// the object's origin
	float4 extent;		// left, right, bottom, top from the origin, along the card
	float4 mask;		// the bitmap's share of its slice across and down, mask texels per world unit
	int4 info;			// x: slice, y: mirrored
};

struct Occluders {
	device const Occluder* list;
	device const int2* polygons;	// per polygon: first index, count
	device const int* indices;
	texture2d_array<float> masks;
	int polygon_count;
	float2 self;					// the figure being shaded: its own card is not in its way
	// Grates (R1): the surface table and wall art (DurandalSurfaces.h), for
	// the see-through sides the walk crosses; null when not built
	device const float4* surfaces;
	texture2d_array<float> walls;
};

// The surface table's float4 per polygon (DurandalSurfaces.h, kPerPolygon)
constant int kSurfacesPerPolygon = 26;

// How much of a ray passes one figure's card between t0 and t1 of the
// segment from `from` along `d`, blurred by the light's size
static float figure_through(thread const Occluders& occ, Occluder o, float3 from, float3 d, float t0, float t1,
							float light_size)
{
	const float dd = dot(d.xy, d.xy);
	if (dd < 1.0)
		return 1.0;
	const float t = dot(o.position.xy - from.xy, d.xy) / dd;
	if (t <= t0 || t > t1 || t >= 0.999)
		return 1.0;
	const float2 across = float2(d.y, -d.x) * rsqrt(dd);
	const float3 p = from + d * t;
	const float blur = light_size * t;	// the light's disc on the card, world units
	const float h = dot(p.xy - o.position.xy, across);
	const float z = p.z - o.position.z;
	if (h < o.extent.x - blur || h > o.extent.y + blur || z < o.extent.z - blur || z > o.extent.w + blur)
		return 1.0;
	float u = (h - o.extent.x) / (o.extent.y - o.extent.x);
	if (o.info.y != 0)
		u = 1.0 - u;
	const float v = (o.extent.w - z) / (o.extent.w - o.extent.z);
	const float lod = log2(max(2.0 * blur * o.mask.z, 1.0));
	constexpr sampler s(filter::linear, mip_filter::linear, address::clamp_to_zero);
	return 1.0 - occ.masks.sample(s, float2(u * o.mask.x, v * o.mask.y), uint(o.info.x), level(lod)).a;
}

// How much of a light passes the figures in polygon `poly`, for the part of
// the segment from `from` along `d` between t0 and t1 (the part inside
// that polygon, so a figure listed in two polygons is met once). The light
// has a size: the silhouette is sampled blurred by the light's disc as
// seen from the lit point, so a shadow is sharp where the figure stands
// close and soft further off.
static float figures_between(thread const Occluders& occ, int poly, float3 from, float3 d, float t0, float t1,
							 float light_size)
{
	if (poly < 0 || poly >= occ.polygon_count)
		return 1.0;
	const int2 range = occ.polygons[poly];
	const float dd = dot(d.xy, d.xy);
	if (range.y == 0 || dd < 1.0)
		return 1.0;
	const float2 across = float2(d.y, -d.x) * rsqrt(dd);
	float through = 1.0;
	for (int k = 0; k < range.y; ++k) {
		const Occluder o = occ.list[occ.indices[range.x + k]];
		if (all(abs(o.position.xy - occ.self) < 0.5))
			continue;
		through *= figure_through(occ, o, from, d, t0, t1, light_size);
		if (through < 0.01)
			return 0.0;
	}
	return through;
}

// light_reaches with the figures in the way, and the end checked: a
// segment that ends inside a polygon's outline but above its ceiling or
// below its floor ends in a room over or under it (stacked rooms leaked
// light through each other: found by scripts/trace-spike.swift)
static float light_transmittance(device const float4* map, int poly, float3 from, float3 to, int light_poly,
								 float light_size, thread const Occluders& occ, bool figures = true)
{
	const float3 d = to - from;
	float t_prev = 0.0;
	float through = 1.0;
	for (int step = 0; step < 24; ++step) {
		if (poly < 0)
			return 0.0;
		// The light's own polygon: the rest of the segment is inside it (as
		// light_reaches, before its edges are looked at)
		if (poly == light_poly)
			return figures ? through * figures_between(occ, poly, from, d, t_prev, 1.0, light_size) : through;
		const float4 h = map[poly * 9];
		const int n = int(h.x);
		float best = 2.0;
		int next = -1, edge = -1;
		float along = 0.0;
		for (int i = 0; i < n; ++i) {
			const float4 e0 = map[poly * 9 + 1 + i];
			const float4 e1 = map[poly * 9 + 1 + (i + 1 == n ? 0 : i + 1)];
			const float2 e = e1.xy - e0.xy;
			const float den = d.x * e.y - d.y * e.x;
			if (abs(den) < 1e-6)
				continue;
			const float2 w = e0.xy - from.xy;
			const float t = (w.x * e.y - w.y * e.x) / den;
			const float s = (w.x * d.y - w.y * d.x) / den;
			if (t > t_prev + 1e-4 && t < best && s >= -1e-3 && s <= 1.0 + 1e-3) {
				best = t;
				next = int(e0.z);
				edge = i;
				along = s;
			}
		}
		if (figures) {
			through *= figures_between(occ, poly, from, d, t_prev, min(best, 1.0), light_size);
			if (through <= 0.0)
				return 0.0;
		}
		if (best > 1.0)
			return (to.z >= h.y - 1.0 && to.z <= h.z + 1.0) ? through : 0.0;
		// Soft edges (Rampant): the light has a size, so near an edge that
		// cuts it off part of its disc is still seen. r: the disc's radius
		// where the segment crosses the edge, as the lit point sees it
		const float r = max(light_size * best, 1.0);
		const float4 e0 = map[poly * 9 + 1 + edge];
		const float4 e1 = map[poly * 9 + 1 + (edge + 1 == n ? 0 : edge + 1)];
		const float len = length(e1.xy - e0.xy);
		const float to_start = along * len, to_end = (1.0 - along) * len;
		const bool open_before = map[poly * 9 + 1 + (edge == 0 ? n - 1 : edge - 1)].z >= 0.0;	// the edge meeting this one at its start
		const bool open_after = e1.z >= 0.0;													// and at its end
		if (next < 0) {
			// A solid wall: near an end where an opening begins (a door
			// jamb), the light passes round it in part
			float round_corner = 0.0;
			if (open_before)
				round_corner = max(round_corner, smoothstep(-r, r, -to_start));
			if (open_after)
				round_corner = max(round_corner, smoothstep(-r, r, -to_end));
			return through * round_corner;
		}
		const float z = mix(from.z, to.z, best);
		const float4 hn = map[next * 9];
		const float lo = max(h.y, hn.y), hi = min(h.z, hn.z);
		const float margin = min(z - lo, hi - z);	// inside the opening's height, > 0
		if (margin < -r)
			return 0.0;	// a step, ledge or lower ceiling in the way
		through *= smoothstep(-r, r, margin);
		// An opening's side beside a solid wall (the jamb) shades it too
		if (!open_before)
			through *= smoothstep(-r, r, to_start);
		if (!open_after)
			through *= smoothstep(-r, r, to_end);
		// Grates: a see-through side here lets light through its gaps only
		if (occ.surfaces) {
			const float4 g = occ.surfaces[1 + poly * kSurfacesPerPolygon + 18 + edge];
			if (g.x > -0.5) {
				constexpr sampler gs(filter::linear, mip_filter::linear, address::repeat);
				const float2 uv = float2(g.y + to_start, g.z - z) / 1024.0;
				const float lod = log2(max(2.0 * r * (128.0 / 1024.0), 1.0));
				through *= 1.0 - occ.walls.sample(gs, uv, uint(g.x), level(lod)).a;
			}
		}
		if (through <= 0.001)
			return 0.0;
		poly = next;
		t_prev = best;
	}
	return through;
}

// Every world fragment writes its colour and its glow (E1): the emissive
// part of the colour, in the same display units, fogged, for EDR and bloom
// (DurandalGL's output pass). Glow alpha follows colour alpha, so the two
// blend alike.
// Each also records its distance from the viewer (colour 2, kept in tile
// memory for the pass): liquid surfaces read it to know how far the view
// travels under them (W1).
struct WorldFrag {
	float4 color [[color(0)]];
	float4 glow [[color(1)]];
	// Distance (W1, R32F). Round 12: ambient shadows, the liquid murk and the
	// distance shade all read this image after the pass, so a blended draw
	// writes it all or nothing per texel: alpha 1 where the texel is over
	// half opaque, else 0 (build_pipelines blends it by that alpha), and a
	// sprite's transparent texels leave what is behind them. Liquid
	// surfaces, the weapon in hand and landscape surfaces (the sky is drawn
	// on the level's own ceiling and wall polygons, steps and all, and
	// their creases shaded lines into it) write the far value, as the sky:
	// no ambient shadows or distance shade on them, they occlude nothing,
	// and the MSAA average at their edges stays far (a marker nearer than
	// its neighbours averaged into phantom surfaces along the water line).
	float4 distance [[color(2)]];
};

struct PackedVertex {
	packed_float3 position;
	packed_float2 texcoord;
};

struct WorldOut {
	float4 position [[position]];
	float2 texcoord [[user(texcoord)]];
	float3 view_xy [[user(view_xy)]];
	float fog_distance [[user(fog_distance)]];
	float classic_depth [[user(classic_depth)]];
	float3 rel_dir [[user(rel_dir)]];
	float3 world [[user(world)]];
	float clip [[clip_distance]] [3];
};

// The fragment stage's view of WorldOut (clip distances are vertex-only)
struct WorldIn {
	float4 position [[position]];
	float2 texcoord [[user(texcoord)]];
	float3 view_xy [[user(view_xy)]];
	float fog_distance [[user(fog_distance)]];
	float classic_depth [[user(classic_depth)]];
	float3 rel_dir [[user(rel_dir)]];
	float3 world [[user(world)]];
};

// The world image is 8-bit: a fixed, zero-mean dither of under one level
// keeps smooth gradients (liquid murk, haze) from banding. Interleaved
// gradient noise (Jimenez), the same for a pixel every frame, so nothing
// shimmers; Marathon's own banded shading is unaffected.
static float3 world_dither(float2 pixel)
{
	const float n = fract(52.9829189 * fract(0.06711056 * pixel.x + 0.00583715 * pixel.y));
	return float3(n - 0.5) / 255.0;
}

// `exact` (>= 0): the fragment's own distance from the viewer, in place of
// the vertices' distances blended across the triangle, which run long on
// big surfaces close to the viewer (traced ambient shadows place points in
// the map by it, and a wall a few units off put them behind it)
static WorldFrag world_frag(WorldIn in, float4 color, float3 glow = float3(0.0), float distance_mode = 1.0,
							float exact = -1.0)
{
	WorldFrag o;
	o.color = float4(color.rgb + world_dither(floor(in.position.xy)), color.a);
	o.glow = float4(glow, color.a);
	o.distance = float4((distance_mode < 0.0 || distance_mode > 1.5) ? 1.0e9 : (exact >= 0.0 ? exact : in.fog_distance), 0.0, 0.0,
						(distance_mode != 0.0 && color.a > 0.5) ? 1.0 : 0.0);
	return o;
}

// Volumetric fog (V1): the light the haze adds between the viewer and this
// pixel at `distance` (rgb) and how much of what is there still shows (a),
// from the fog volume (volume_kernel)
static float4 fog_at(constant Uniforms& u, texture3d<float> volume, float2 pixel, float distance)
{
	if (u.volume == 0)
		return float4(0.0, 0.0, 0.0, 1.0);
	constexpr sampler s(filter::linear, address::clamp_to_edge);
	const float k = log(max(distance, 1.0) / u.volume_screen.w) * u.volume_screen.z;
	return volume.sample(s, float3(pixel * u.volume_screen.xy, (k + 0.5) / float(volume.get_depth())));
}

static WorldFrag fogged_frag(constant Uniforms& u, texture3d<float> volume, WorldIn in, float4 color,
							 float3 glow = float3(0.0))
{
	const float4 v = fog_at(u, volume, in.position.xy, in.fog_distance);
	return world_frag(in, float4(color.rgb * v.a + v.rgb, color.a), glow * v.a, u.distance_mode,
					  u.rampant.w != 0 ? length(in.world - u.camera.xyz) : -1.0);
}

vertex WorldOut world_vertex(const device PackedVertex* vertices [[buffer(0)]],
							 constant Uniforms& u [[buffer(1)]],
							 uint vid [[vertex_id]])
{
	WorldOut out;
	const float4 p = u.model * float4(float3(vertices[vid].position), 1.0);
	const float4 eye = u.modelview * p;
	float4 clip = u.projection * eye;
	clip.z = clip.z + u.depth_offset * clip.z / 65536.0;
	out.classic_depth = clip.z / 8192.0;
	out.position = float4(clip.x, clip.y, (clip.z + clip.w) * 0.5, clip.w);

	const float4 tc = u.texture_matrix * float4(float2(vertices[vid].texcoord), 0.0, 1.0);
	out.texcoord = tc.xy;

	out.view_xy = float3(0.0);
	if (length_squared(u.normal.xyz) > 0.0) {
		const float3x3 nm = float3x3(u.modelview[0].xyz, u.modelview[1].xyz, u.modelview[2].xyz);
		const float3 n = normalize(nm * u.normal.xyz);
		const float3 t = normalize(nm * u.tangent.xyz);
		const float3 b = normalize(cross(n, t) * u.tangent.w);
		const float3x3 tbn = transpose(float3x3(t, b, n));
		const float3 view_dir = tbn * eye.xyz;
		out.view_xy = -(u.texture_matrix * float4(view_dir, 1.0)).xyz;
	}
	// The weapon in hand (distance_mode -1) is right in front of the face,
	// not where its screen-space vertices put it (1-2 WU: in thick murk it
	// vanished into a dark blob)
	out.fog_distance = u.distance_mode < 0.0 ? 128.0 : length(eye.xyz);
	out.rel_dir = eye.xyz;
	out.world = p.xyz;

	for (int i = 0; i < 3; ++i)
		out.clip[i] = (u.clip_mask & (1u << i)) ? dot(u.clip_planes[i], p) : 1.0;
	return out;
}

static float fog_factor(constant Uniforms& u, float distance)
{
	if (u.fog_mode == 0.0)
		return clamp((u.fog_end - distance) / (u.fog_end - u.fog_start), 0.0, 1.0);
	else if (u.fog_mode == 1.0)
		return clamp(exp(-u.fog_density * distance), 0.0, 1.0);
	else if (u.fog_mode == 2.0)
		return clamp(exp(-u.fog_density * u.fog_density * distance * distance), 0.0, 1.0);
	return 1.0;
}

// `extra`: dynamic light (E2) added to the surface light, as a brighter
// light level would be
static float3 classic_intensity(constant Uniforms& u, float classic_depth, float extra = 0.0)
{
	const float ml = clamp(u.self_luminosity + u.flare - classic_depth, 0.0, 1.0);
	const float3 ambient = min(u.color.rgb + extra, float3(1.0));
	float3 intensity;
	if (ambient.r > ml)
		intensity = ambient + (ml * 0.5);
	else
		intensity = (ambient * 0.5) + ml;
	return clamp(intensity, u.glow, 1.0);
}

static float2 wobbled(constant Uniforms& u, WorldIn in)
{
	float2 tc = in.texcoord;
	if (u.pulsate == 0.0 && u.wobble == 0.0)
		return tc;
	const float3 n = normalize(in.view_xy);
	tc += float2(n.y * -u.pulsate, n.x * u.pulsate);
	tc += float2(n.y * -u.wobble * tc.y, u.wobble * tc.y);
	return tc;
}

// Pipelines for solid surfaces (unblended, fully opaque texture) are built
// with this false: a shader that can never discard lets the GPU skip the
// hidden layers of Marathon's back-to-front drawing (hidden surface
// removal) instead of shading every one.
constant bool kMayDiscard [[function_constant(0)]];

static void alpha_test(constant Uniforms& u, float a)
{
	if (kMayDiscard && u.alpha_threshold >= 0.0 && a <= u.alpha_threshold)
		discard_fragment();
}

// Texel-faithful lighting (roadmap L2): the headlight depth and fog
// distance at the centre of the source texel under this pixel, found from
// screen-space derivatives, so each texel takes exactly one shade. The
// derivatives are taken unconditionally (they need uniform control flow).
struct Lighting {
	float depth;
	float fog_distance;
	float3 world;		// also at the texel centre, for dynamic lights
	float3 normal;		// the face's, towards the viewer
};

static Lighting texel_lighting(constant Uniforms& u, WorldIn in, float2 texel)
{
	const float2 dtx = dfdx(texel), dty = dfdy(texel);
	const float ddx = dfdx(in.classic_depth), ddy = dfdy(in.classic_depth);
	const float fdx = dfdx(in.fog_distance), fdy = dfdy(in.fog_distance);
	const float3 wdx = dfdx(in.world), wdy = dfdy(in.world);
	float3 n = cross(wdx, wdy);
	n = length_squared(n) > 0.0 ? normalize(n) : float3(0.0, 0.0, 1.0);
	if (dot(n, u.camera.xyz - in.world) < 0.0)
		n = -n;
	Lighting l = { in.classic_depth, in.fog_distance, in.world, n };
	if (u.texel_lighting == 0)
		return l;
	const float det = dtx.x * dty.y - dtx.y * dty.x;
	if (abs(det) < 1e-12)
		return l;
	// Screen offset (a, b) with texel + a*dtx + b*dty = texel centre
	const float2 d = floor(texel) + 0.5 - texel;
	float a = (d.x * dty.y - d.y * dty.x) / det;
	float b = (dtx.x * d.y - dtx.y * d.x) / det;
	a = clamp(a, -256.0, 256.0);
	b = clamp(b, -256.0, 256.0);
	l.depth += a * ddx + b * ddy;
	l.fog_distance += a * fdx + b * fdy;
	l.world += a * wdx + b * wdy;
	return l;
}

// Dynamic lights (E2): projectiles, explosions and muzzle flashes add to the
// surface's light before shading, so they band like Marathon's own light
// levels. `amount` is the added light level; `tint` their colour. A light
// only reaches the side of a surface's plane it is on.
struct DynamicLight {
	float amount;
	float3 tint;
};

// `visible(i, amount)`: how much of light i reaches the point, 0 to 1;
// `amount` is what it would add unshadowed
template <typename Visible>
static DynamicLight dynamic_light_with(constant Uniforms& u, constant Light* lights, Lighting l, bool facing,
									   Visible visible)
{
	DynamicLight d = { 0.0, float3(1.0) };
	float3 colour = float3(0.0);
	// Only the lights that can reach this surface (culled per draw on the
	// CPU), in the same order as before, so the sums are the same
	for (uint m = u.culling.x & ((1u << u.light_count) - 1u); m != 0u; m &= m - 1u) {
		const uint i = ctz(m);
		const float3 to = lights[i].position_radius.xyz - l.world;
		const float r = lights[i].position_radius.w;
		const float dist2 = length_squared(to);
		if (dist2 >= r * r)
			continue;
		float f = 1.0 - dist2 / (r * r);
		f *= f;
		if (facing) {
			const float nl = dot(l.normal, to) * rsqrt(max(dist2, 1.0));
			if (nl <= 0.0)
				continue;
			f *= 0.35 + 0.65 * nl;
		}
		const float full = lights[i].colour_strength.w * f;
		const float seen = visible(i, full);
		if (seen <= 0.0)
			continue;
		const float a = full * seen;
		d.amount += a;
		colour += lights[i].colour_strength.rgb * a;
	}
	if (d.amount > 0.0) {
		const float3 c = colour / d.amount;
		d.tint = c / max(max(c.r, max(c.g, c.b)), 1e-3);
	}
	return d;
}

static DynamicLight dynamic_light(constant Uniforms& u, constant Light* lights, device const float4* map,
								  Lighting l, bool facing, bool shadows = true)
{
	return dynamic_light_with(u, lights, l, facing, [&](uint i, float) {
		return (shadows && u.shadows && !light_reaches(map, u.polygon, l.world, lights[i].position_radius.xyz, lights[i].info.x))
			? 0.0 : 1.0;
	});
}

// With traced shadows (R1) the figures in the way count, softly
static DynamicLight dynamic_light(constant Uniforms& u, constant Light* lights, device const float4* map,
								  thread const Occluders& occ, Lighting l, bool facing)
{
	return dynamic_light_with(u, lights, l, facing, [&](uint i, float amount) {
		if (!u.shadows)
			return 1.0;
		const float3 to = lights[i].position_radius.xyz;
		if (u.rampant.x != 0) {
			// Light too faint to see is not walked at all, and faint light
			// passes the walls only: a firing line of sixteen flashes walked
			// every figure for every pixel (world pass p99 17 ms)
			if (amount < 0.03)
				return 0.0;
			const float size = lights[i].info.y > 0 ? float(lights[i].info.y) : 150.0;
			return light_transmittance(map, u.polygon, l.world, to, lights[i].info.x, size, occ,
									   amount >= 0.15 && lights[i].info.z != 0);
		}
		return light_reaches(map, u.polygon, l.world, to, lights[i].info.x) ? 1.0 : 0.0;
	});
}

// Liquids (W1): the moving web of light a rippling surface throws on what
// is under it; bright where two drifting wave lattices both cross zero
static float caustic_pattern(float2 p, float t)
{
	const float2 a = p + float2(sin(p.y * 0.9 + t * 0.7), sin(p.x * 1.1 - t * 0.6)) * 0.8;
	const float2 b = p * 1.3 + float2(sin(p.y * 1.2 - t * 0.5), sin(p.x * 0.8 + t * 0.8)) * 0.8;
	const float l = min(abs(sin(a.x) * sin(a.y)), abs(sin(b.x) * sin(b.y)));
	return pow(1.0 - l, 6.0);
}

// Caustics as light on surfaces below a liquid's surface, fading with depth
static DynamicLight add_caustics(constant Uniforms& u, DynamicLight d, Lighting l)
{
	if (u.caustics <= 0.0 || l.world.z >= u.media_height)
		return d;
	const float depth = u.media_height - l.world.z;
	// As bright as the room's own light allows: a dark room's pool stays dark
	const float c = caustic_pattern(l.world.xy / 600.0, u.time / 30.0) * u.caustics * exp(-depth / 2048.0)
		* saturate(u.color.r * 1.25);
	if (c <= 0.0)
		return d;
	const float total = d.amount + c;
	d.tint = (d.amount > 0.0 ? d.tint * d.amount : float3(0.0)) + float3(1.0) * c;
	d.tint /= total;
	d.amount = total;
	return d;
}

// Dynamic light only adds: what the surface gains from being lit brighter
// (lit - unlit, both through the usual shading), in the lights' colour
static float3 add_light(float3 unlit, float3 lit, DynamicLight d)
{
	return d.amount > 0.0 ? unlit + max(lit - unlit, float3(0.0)) * d.tint : unlit;
}

// Contact shadows: a soft pool of shade on the floor under each item,
// monster and piece of scenery, so they sit on the ground rather than
// float over it. The light multiplier for this point (1: none); only
// upward-facing surfaces at the object's floor height receive it.
static float contact_shadow(constant Uniforms& u, constant Caster* casters, Lighting l)
{
	if (u.caster_count == 0 || l.normal.z < 0.7)
		return 1.0;
	float lit = 1.0;
	// Only the casters that can reach this surface (culled per draw)
	for (uint m = u.caster_count >= 32u ? u.culling.y : u.culling.y & ((1u << u.caster_count) - 1u); m != 0u; m &= m - 1u) {
		const uint i = ctz(m);
		const float4 p = casters[i].position_radius;
		const float dz = abs(l.world.z - p.z);
		if (dz >= 32.0)
			continue;
		const float d2 = length_squared(l.world.xy - p.xy);
		if (d2 >= p.w * p.w)
			continue;
		float f = 1.0 - d2 / (p.w * p.w);
		f *= f;
		lit *= 1.0 - casters[i].info.x * f * (1.0 - dz / 32.0);
	}
	return max(lit, 0.3);
}

// Light redistribution (E4): for this surface point, the colour tint (rgb)
// and the factor on its brightness (a), from the surface cache: the light
// arriving at its lumel against its patch's average, clamped
// Any patch at any point on it (the bake's bounce reads the surfaces it
// hits this way, so light is passed on as it is drawn)
static float4 patch_light(float4 range, device const Patch* patches, device const float4* averages,
						  texture2d<float> radiance, int patch, float3 world)
{
	if (patch < 0)
		return float4(1.0);
	const float4 average = averages[patch];
	if (average.w < 1.0)
		return float4(1.0);	// not baked yet
	const Patch pa = patches[patch];
	const float3 r = world - pa.origin.xyz;
	const float2 l = clamp(float2(dot(r, pa.u.xyz), dot(r, pa.v.xyz)), float2(0.5), float2(pa.rect.zw) - 0.5);
	constexpr sampler s(filter::linear, address::clamp_to_edge);
	// Four bilinear taps a lumel apart (about a 3x3 tent): each lumel
	// keeps a few per cent of noise from its rays, which a lumel read alone
	// showed as blobs where the rays find small bright surfaces (a lift
	// shaft in Marathon, 3 Oct 2026); averaged, it no longer shows
	const float2 size = float2(radiance.get_width(), radiance.get_height());
	const float2 lo = float2(0.5), hi = float2(pa.rect.zw) - 0.5;
	float4 e = float4(0.0);
	for (int k = 0; k < 4; ++k) {
		const float2 o = float2((k & 1) ? 0.75 : -0.75, (k & 2) ? 0.75 : -0.75);
		e += radiance.sample(s, (float2(pa.rect.xy) + clamp(l + o, lo, hi)) / size, level(0));
	}
	e *= 0.25;
	if (e.a < 0.05)
		return float4(1.0);
	const float3 here = e.rgb / e.a;
	const float3 luma = float3(0.2126, 0.7152, 0.0722);
	const float lh = dot(here, luma), la = dot(average.rgb, luma);
	// Both measured against a small floor: where a room gives off almost
	// nothing (Marathon's unlit rooms, seen only by the player's own close
	// light), what little arrives is stray light from far openings, and
	// the bare ratio swung from lumel to lumel between the clamps: blobs
	// (a lift, 3 Oct 2026). There it stays near 1; lit rooms are as before
	constexpr float kFloor = 0.02;
	const float ratio = clamp((lh + kFloor) / (la + kFloor), 1.0 - range.x, 1.0 + range.y);
	if (la < 1e-4 || lh < 1e-5)
		return float4(1.0, 1.0, 1.0, ratio);
	const float confidence = la / (la + kFloor);
	const float3 tint = clamp(mix(float3(1.0), (here / lh) / (average.rgb / la), range.z * confidence),
							  float3(1.0 - range.w), float3(1.0 + range.w));
	return float4(tint, ratio);
}

static float4 redistribution(constant Uniforms& u, device const Patch* patches, device const float4* averages,
							 texture2d<float> radiance, float3 world)
{
	if (u.gi == 0)
		return float4(1.0);
	return patch_light(u.gi_range, patches, averages, radiance, u.patch, world);
}

// Bounced Light (R4): a figure takes the redistributed light of the floor
// under it and the ceiling over it, at its own position (a figure in the
// bright pool by a lamp is brighter, one in a dark corner darker)
static float4 figure_light(constant Uniforms& u, device const Patch* patches, device const float4* averages,
						   texture2d<float> radiance, float3 world)
{
	if (u.gi == 0 || u.figure_patches.x < 0)
		return float4(1.0);
	const float4 floor_light = patch_light(u.gi_range, patches, averages, radiance, u.figure_patches.x, world);
	const float4 ceiling_light = u.figure_patches.y >= 0
		? patch_light(u.gi_range, patches, averages, radiance, u.figure_patches.y, world) : floor_light;
	return mix(floor_light, ceiling_light, 0.35);
}

// Crisp filtering (roadmap L4) for true-colour textures: sharp bilinear.
// Inside a texel the sample stays at its centre (pixel-honest texels);
// only across the texel's edge, within one screen pixel, does it blend
// with its neighbour. Minified, it becomes ordinary trilinear/anisotropic
// filtering (the gradients are the unmodified ones, so the mip level is
// right). Needs a linear, mipmapped, anisotropic sampler (DurandalMetal).
static float4 crisp_sample(constant Uniforms& u, texture2d<float> tex, sampler smp, float2 tc)
{
	const float2 size = float2(tex.get_width(), tex.get_height());
	const float2 px = tc * size;
	const float2 fw = max(fwidth(px), float2(1e-4));
	const float2 gx = dfdx(tc), gy = dfdy(tc);
	if ((u.filtering & 1u) == 0)
		return tex.sample(smp, tc);
	const float2 p = px - 0.5;
	const float2 i = floor(p);
	const float2 w = clamp((p - i - 0.5) / fw + 0.5, 0.0, 1.0);
	return tex.sample(smp, (i + w + 0.5) / size, gradient2d(gx, gy));
}

// Surface relief (M1) and HD art normal maps share this: how the light
// is shaped at a texel. tilt is the normal less the face's (unnormalised,
// linear in the height gradient); head, glint and fill are the three
// lighting terms relief_light fills in.
struct Relief {
	float2 shift;
	float3 normal;
	float3 tilt;	// normal less the face's, unnormalised: linear in the height gradient
	float head;
	float glint;
	float fill;		// the room's light on the relief, as if from the room side, in front and above
};

// The relief's light, seen from the texel's centre (one shade per texel):
// how much of the headlight it catches against the flat face (head), its
// highlight (glint), and the room's light on it (fill). The room's light
// has no direction in Marathon; shaped gently as if from above and in
// front, it averages out over the texture.
// Relief shapes a surface's light but never changes its amount: each term
// is linear in the relief's tilt (the height gradient, which sums to zero
// over a texture) at a gain low enough that the safety clamps hardly ever
// engage. Painted art's gradients are skewed (gentle rises, sharp drops),
// so any clipping of the swing would shift a surface's average brightness,
// which is what drew a rectangle where the relief faded with distance.
static void relief_light(constant Uniforms& u, thread Relief& r, float3 face, float3 world)
{
	const float3 V = normalize(u.camera.xyz - world);
	// The headlight: brighter where the relief turns to the viewer, darker
	// where it turns away
	r.head = 1.0 + clamp(0.5 * dot(r.tilt, V), -0.7, 0.7);
	// A highlight on a surface facing the viewer, brighter where the relief
	// turns to the viewer and dimmer where it turns away: the mirror lobe
	// (2c^2 - 1)^24 of the face, sloped by the tilt (slope held to 0.6)
	const float c0 = dot(face, V);
	const float lobe = 2.0 * c0 * c0 - 1.0;
	const float slope = lobe > 0.0 ? min(24.0 * pow(lobe, 23.0) * 4.0 * c0, 0.6) : 0.0;
	r.glint = clamp(slope * dot(r.tilt, V), -0.7, 0.7) * 0.35 * r.glint;
	// The room's light as if from the room's side of the surface, in front
	// of it and a little above: floors are shaped as if lit from above,
	// ceilings from below, walls from the room
	const float3 A = normalize(face + V + float3(0.0, 0.0, 0.5));
	r.fill = 1.0 + clamp(0.35 * dot(r.tilt, A), -0.5, 0.5);
}

// The relief's terms as one factor on the finished intensity, as
// classic_shade applies them (each term averages to zero over a texture,
// so a surface keeps its authored brightness)
static float relief_factor(constant Uniforms& u, float classic_depth, Relief r)
{
	const float s = clamp(u.self_luminosity + u.flare - classic_depth, 0.0, 1.0);
	const float a = min(u.color.r, 1.0);
	const float total = max(s + a, 1e-3);
	return 1.0 + clamp((r.head - 1.0 + r.glint) * (s / total) + (r.fill - 1.0) * (a / total), -0.7, 0.7);
}

// HD art normal maps: a pack's tangent-space normal map (x along the
// texture's across, y up the image, z out of the face: the convention
// upstream's bump path and the community packs use) tilts the light on a
// replacement wall as surface relief (M1) does on the shapes file's art,
// with the same three terms (relief_light), and dynamic lights see the
// tilted normal. The tangent frame comes from screen derivatives, as for
// relief; the image's up is the opposite of the texture's down (-B), or
// its down for a map in the other convention (normal_map 2).
static Relief normal_map_relief(constant Uniforms& u, WorldIn in, texture2d<float> bump, sampler smp, float2 tc, Lighting l)
{
	Relief r = { float2(0.0), l.normal, float3(0.0), 1.0, 0.0, 1.0 };
	if (u.normal_map == 0u)
		return r;
	const float2 dtx = dfdx(tc), dty = dfdy(tc);
	const float det = dtx.x * dty.y - dtx.y * dty.x;
	if (abs(det) < 1e-20)
		return r;
	const float3 wdx = dfdx(in.world), wdy = dfdy(in.world);
	const float3 T = normalize((wdx * dty.y - wdy * dtx.y) / det);
	const float3 B = normalize((wdy * dtx.x - wdx * dty.x) / det);
	const float3 n = bump.sample(smp, tc).rgb * 2.0 - 1.0;
	const float y_sign = (u.normal_map == 2u) ? 1.0 : -1.0;
	r.tilt = (T * n.x + B * n.y * y_sign) * (u.normal_gain / max(n.z, 0.25));
	r.normal = normalize(l.normal + r.tilt);
	r.glint = 0.6;
	relief_light(u, r, l.normal, l.world);
	return r;
}

// HD art bloom: a replacement image's share of the glow image, as the
// OpenGL bloom pass computed it (wall_bloom.frag): the colour times
// clamp(clamp(light, glow, 1) x scale + shift, 0, 1). Zero for the shapes
// file's own textures, whose scale and shift stay 0.
static float3 pack_glow(constant Uniforms& u, float3 color, float3 intensity)
{
	if (u.bloom_scale == 0.0 && u.bloom_shift == 0.0)
		return float3(0.0);
	return color * saturate(clamp(intensity, u.glow, 1.0) * u.bloom_scale + u.bloom_shift);
}

// HD art glow gain: the 8-bit path glows a self-luminous colour at full
// strength however it is lit, and those colours are the palette's
// brightest; a replacement sprite draws the same bolt or lamp as soft,
// mid-toned pixels, so its glow comes out weaker. The gain lifts a
// replacement's glow in proportion to its brightness: a bright pixel by
// up to the gain, a dim one hardly at all.
static float3 glow_gain(constant Uniforms& u, float3 g)
{
	if (u.glow_gain <= 1.0)
		return g;
	const float luma = dot(g, float3(0.2126, 0.7152, 0.0722));
	return g * (1.0 + (u.glow_gain - 1.0) * saturate(luma));
}

fragment WorldFrag wall_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
							  constant Light* lights [[buffer(2)]], device const float4* map [[buffer(3)]],
							  constant Caster* casters [[buffer(4)]], texture3d<float> volume [[texture(3)]],
							  device const Patch* patches [[buffer(5)]], device const float4* averages [[buffer(6)]],
							  texture2d<float> radiance [[texture(4)]],
							  texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]],
							  texture2d<float> bump [[texture(5)]],
							  device const Occluder* occluders [[buffer(7)]], device const int2* occluder_polygons [[buffer(8)]],
							  device const int* occluder_indices [[buffer(9)]], texture2d_array<float> masks [[texture(6)]],
							  device const float4* surfaces [[buffer(10)]], texture2d_array<float> walls [[texture(7)]])
{
	const Occluders occ = { occluders, occluder_polygons, occluder_indices, masks, u.rampant.y, float2(u.figure_patches.zw),
							u.rampant.z != 0 || (u.rampant.x & 2) != 0 ? surfaces : nullptr, walls };
	const float2 tc = wobbled(u, in);
	const Lighting l = texel_lighting(u, in, tc * float2(tex.get_width(), tex.get_height()));
	// HD art: the pack's normal map shapes the light; dynamic lights see it
	const Relief rf = normal_map_relief(u, in, bump, smp, tc, l);
	const float4 color = crisp_sample(u, tex, smp, tc);
	// A texel the alpha test will drop is dropped before its lights are
	// walked (every derivative and implicit-LOD sample is above this line)
	alpha_test(u, u.color.a * color.a);
	Lighting lr = l;
	lr.normal = rf.normal;
	DynamicLight dl = dynamic_light(u, lights, map, occ, lr, true);
	dl = add_caustics(u, dl, l);
	const float4 gi = redistribution(u, patches, averages, radiance, l.world);
	const float mod = relief_factor(u, l.depth, rf);
	const float3 intensity = add_light(classic_intensity(u, l.depth) * mod, classic_intensity(u, l.depth, dl.amount) * mod, dl)
		* contact_shadow(u, casters, l) * gi.a * gi.rgb;
	if (u.gi_range.w < 0.0)	// development: DURANDAL_GI_VIEW=1 shows the redistribution's factor (1 = mid grey)
		return world_frag(in, float4(float3(gi.a * 0.5), 1.0), float3(0.0), u.distance_mode);
	const float f = fog_factor(u, l.fog_distance);
	const float4 out = float4(mix(u.fog_color.rgb, color.rgb * intensity, f), u.color.a * color.a);
	// Glow: a pack's bloom share of this image (HD art)
	return fogged_frag(u, volume, in, out, u.write_glow ? pack_glow(u, color.rgb, intensity) * f : float3(0.0));
}

fragment WorldFrag wall_infravision_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
										  texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]])
{
	const float4 color = tex.sample(smp, wobbled(u, in));
	const float avg = (color.r + color.g + color.b) / 3.0;
	const float f = fog_factor(u, in.fog_distance);
	const float4 out = float4(mix(u.fog_color.rgb, u.color.rgb * avg, f), u.color.a * color.a);
	alpha_test(u, out.a);
	return world_frag(in, out, float3(0.0), u.distance_mode);
}

fragment WorldFrag sprite_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
								constant Light* lights [[buffer(2)]], device const float4* map [[buffer(3)]],
								texture3d<float> volume [[texture(3)]],
								device const Patch* patches [[buffer(5)]], device const float4* averages [[buffer(6)]],
								texture2d<float> radiance [[texture(4)]],
								texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]],
								device const Occluder* occluders [[buffer(7)]], device const int2* occluder_polygons [[buffer(8)]],
							  device const int* occluder_indices [[buffer(9)]], texture2d_array<float> masks [[texture(6)]],
							  device const float4* surfaces [[buffer(10)]], texture2d_array<float> walls [[texture(7)]])
{
	const Occluders occ = { occluders, occluder_polygons, occluder_indices, masks, u.rampant.y, float2(u.figure_patches.zw),
							u.rampant.z != 0 || (u.rampant.x & 2) != 0 ? surfaces : nullptr, walls };
	const Lighting l = texel_lighting(u, in, in.texcoord * float2(tex.get_width(), tex.get_height()));
	const float4 color = crisp_sample(u, tex, smp, in.texcoord);
	// A texel the alpha test will drop is dropped before its lights are
	// walked (every derivative and implicit-LOD sample is above this line)
	alpha_test(u, u.color.a * color.a);
	DynamicLight dl;
	if (u.viewer_light.a > 0.0) {
		// The weapon in hand: lit by the lights around the viewer, so the
		// lights at its own (nominal) position are not walked
		dl.amount = u.viewer_light.a;
		dl.tint = u.viewer_light.rgb;
	} else {
		dl = dynamic_light(u, lights, map, occ, l, false);
	}
	const float4 gi = figure_light(u, patches, averages, radiance, l.world);
	const float3 intensity = add_light(classic_intensity(u, l.depth), classic_intensity(u, l.depth, dl.amount), dl) * gi.a * gi.rgb;
	const float f = fog_factor(u, l.fog_distance);
	const float4 out = float4(mix(u.fog_color.rgb, color.rgb * intensity, f), u.color.a * color.a);
	// Glow: projectiles, explosions, flashes and lava scenery (their frame's
	// minimum light), and a pack's bloom share of its image (HD art)
	return fogged_frag(u, volume, in, out,
					   u.write_glow ? glow_gain(u, max(color.rgb * u.emissive, pack_glow(u, color.rgb, intensity))) * f : float3(0.0));
}

fragment WorldFrag sprite_infravision_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
											texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]])
{
	const float4 color = tex.sample(smp, in.texcoord);
	const float avg = (color.r + color.g + color.b) / 3.0;
	const float f = fog_factor(u, in.fog_distance);
	const float4 out = float4(mix(u.fog_color.rgb, u.color.rgb * avg, f), u.color.a * color.a);
	alpha_test(u, out.a);
	return world_frag(in, out, float3(0.0), u.distance_mode);
}

// Marathon's 8-bit shading (roadmap L1; see DurandalShading.h), baked per
// colour table into `shades` (DurandalMetal.mm, bake_shades): 256 pixel
// values across; rows 0-31 are the 32 tables (banded, 0 darkest), rows
// 32-159 the same walk sampled finely for smooth shading, where linear
// filtering between rows blends neighbouring ramp colours. t is the
// table, 0 (darkest) to 31 (brightest), fractional when smooth.
constant int kShadeRows = 160;
constant int kSmoothRows = 128;

// rgb: the colour; a: 1 for self-luminous colours (the same in every row)
static float4 ramp_colour(texture2d<float> shades, uint value, float t, bool banded)
{
	if (banded)
		return shades.read(uint2(value, uint(t)));
	constexpr sampler s(filter::linear, address::clamp_to_edge);
	const float row = 32.0 + t * float(kSmoothRows - 1) / 31.0 + 0.5;
	return shades.sample(s, float2((float(value) + 0.5) / 256.0, row / float(kShadeRows)));
}

// The software renderer's shade: headlight S and surface light A combine
// as max + min/2 (calculate_shading_table)
static float classic_shade(constant Uniforms& u, float classic_depth, float extra = 0.0, float head = 1.0,
						   float glint = 0.0, float fill = 1.0)
{
	const float s = clamp(u.self_luminosity + u.flare - classic_depth, 0.0, 1.0);
	const float a = min(u.color.r + extra, 1.0);
	const float shade = a > s ? a + s * 0.5 : s + a * 0.5;
	// Surface relief (M1) shapes the light as one factor on the finished
	// shade: the headlight's share by head and glint, the room's by fill.
	// Each term averages to zero over a texture, and so does the factor,
	// so a surface keeps its authored brightness (applied inside the
	// max-like mix above, a swing would brighten it on average)
	const float total = max(s + a, 1e-3);
	const float mod = 1.0 + clamp((head - 1.0 + glint) * (s / total) + (fill - 1.0) * (a / total), -0.7, 0.7);
	return shade * mod;
}

static uint2 texel_address(int2 c, int2 size, bool repeat)
{
	if (repeat)
		return uint2(((c % size) + size) % size);
	return uint2(clamp(c, int2(0), size - 1));
}

// One texel of mip level `level` through the ramps: its colour and its
// glow, both premultiplied by its opacity. The shades texture's alpha is 1
// for self-luminous colours; the index texture's third channel is the
// texel's wall-light glow (DurandalGlow.h).
struct Tap {
	float4 c;		// colour, lit by dynamic lights
	float3 c0;		// colour without them
	float3 g;
};

static Tap ramp_texel(constant Uniforms& u, texture2d<uint> indices, texture2d<float> ramps, int2 c, int2 size,
					  uint level, bool repeat, float t, float t0, bool banded)
{
	const uint3 v = indices.read(texel_address(c, size, repeat), level).rgb;
	const float a = float(v.y) / 255.0;
	Tap tap;
	const float4 shaded = ramp_colour(ramps, v.x, t, banded);
	tap.c = float4(shaded.rgb * a, a);
	tap.c0 = (t == t0) ? tap.c.rgb : ramp_colour(ramps, v.x, t0, banded).rgb * a;
	tap.g = float3(0.0);
	// Only texels that can glow need the full-bright colour
	if (u.write_glow && (v.z > 0u || u.emissive > 0.0 || shaded.a > 0.5)) {
		// Self-luminous colours glow fully; wall lights (the texel's glow)
		// as brightly as their surface is lit; objects by their minimum light
		const float4 full = ramps.read(uint2(v.x, 31));
		const float lamp = float(v.z) / 255.0 * saturate(u.color.r);
		tap.g = full.rgb * max(max(full.a, lamp), u.emissive) * a;
	}
	return tap;
}

static Tap mix_taps(Tap a, Tap b, float f)
{
	Tap t;
	t.c = mix(a.c, b.c, f);
	t.c0 = mix(a.c0, b.c0, f);
	t.g = mix(a.g, b.g, f);
	return t;
}

static int2 level_size(int2 size, uint level)
{
	return max(size >> int(level), int2(1));
}

// Surface relief (M1; DurandalRelief.h). The index texture's fourth
// channel is a height (1-255; 0 none). Per texel (heights read at texel
// centres, so it stays texel-faithful): a normal from the neighbouring
// heights in the surface's texture frame (found from screen derivatives);
// how much of the headlight the relief catches against the flat face
// (head); a highlight where the relief turns towards it more than the face
// (glint); and a restrained parallax shift of the texel looked up, under a
// texel either way, up close only.

static float relief_height(texture2d<uint> indices, int2 c, int2 size, uint level, bool repeat)
{
	const uint a = indices.read(texel_address(c, level_size(size, level), repeat), level).a;
	return a == 0u ? -1.0 : float(a - 1u) / 254.0;
}

static Relief surface_relief(constant Uniforms& u, WorldIn in, texture2d<uint> indices, float2 texel, int2 size,
							 bool repeat)
{
	const float2 dtx = dfdx(texel), dty = dfdy(texel);
	const float3 wdx = dfdx(in.world), wdy = dfdy(in.world);
	float3 face = cross(wdx, wdy);
	face = length_squared(face) > 0.0 ? normalize(face) : float3(0.0, 0.0, 1.0);
	if (dot(face, u.camera.xyz - in.world) < 0.0)
		face = -face;
	Relief r = { float2(0.0), face, float3(0.0), 1.0, 0.0, 1.0 };
	const float det = dtx.x * dty.y - dtx.y * dty.x;
	if (u.relief <= 0.0 || abs(det) < 1e-12)
		return r;
	const float3 T = normalize((wdx * dty.y - wdy * dtx.y) / det);	// the texture's across, in the world
	const float3 B = normalize((wdy * dtx.x - wdx * dty.x) / det);	// and its down
	const float lod = log2(max(max(length(dtx), length(dty)), 1e-6));
	// Relief shows up close; it fades out as texels shrink towards a
	// pixel and costs nothing beyond
	const float strength = u.relief * saturate(0.5 - lod);
	if (strength <= 0.0)
		return r;
	// Always the full-detail heights: relief has faded out before a
	// coarser level would be needed, and switching levels draws a line
	const uint level = 0u;
	const int2 c = int2(floor(texel / float(1u << level)));
	const float h = relief_height(indices, c, size, level, repeat);
	if (h < 0.0)
		return r;
	float hx0 = relief_height(indices, c - int2(1, 0), size, level, repeat);
	float hx1 = relief_height(indices, c + int2(1, 0), size, level, repeat);
	float hy0 = relief_height(indices, c - int2(0, 1), size, level, repeat);
	float hy1 = relief_height(indices, c + int2(0, 1), size, level, repeat);
	hx0 = hx0 < 0.0 ? h : hx0; hx1 = hx1 < 0.0 ? h : hx1;
	hy0 = hy0 < 0.0 ? h : hy0; hy1 = hy1 < 0.0 ? h : hy1;
	const float3 V = normalize(u.camera.xyz - in.world);
	if (level == 0u) {
		// At most half a texel either way, so texels keep their shape
		const float3 vt = float3(dot(V, T), dot(V, B), max(dot(V, face), 0.5));
		// Faded to nothing by the time texels are a pixel, so there is
		// no step where it stops
		r.shift = clamp((h - 0.5) * 0.8 * strength * saturate(-2.0 * lod) * vt.xy / vt.z, float2(-0.5), float2(0.5));
	}
	const float2 g = float2(hx1 - hx0, hy1 - hy0) * 0.5 * 2.4 * strength;
	r.tilt = -T * g.x - B * g.y;
	r.normal = normalize(face + r.tilt);
	r.glint = strength * max(h, 1e-3);	// the highlight's strength (> 0: relief here); shaped by relief_light
	return r;
}

static WorldFrag ramp_shade(WorldIn in, constant Uniforms& u, constant Light* lights, device const float4* map,
							constant Caster* casters, texture3d<float> volume, device const Patch* patches,
							device const float4* averages, texture2d<float> radiance, texture2d<uint> indices,
							texture2d<float> ramps, float2 tc, bool repeat, thread const Occluders& occ)
{
	const int2 size = int2(indices.get_width(), indices.get_height());
	Relief rf = { float2(0.0), float3(0.0), float3(0.0), 1.0, 0.0, 1.0 };
	const float2 texel0 = tc * float2(size);
	if (repeat)
		rf = surface_relief(u, in, indices, texel0, size, repeat);
	// Relief's parallax shift moves the lookup, not the texel size: the
	// filtering below is judged on the unshifted coordinates (the shift
	// changes from texel to texel, and its derivatives would push relief
	// surfaces onto coarser mips and change their average colour)
	const float2 texel = texel0 + rf.shift;
	// The texel's lighting position comes from the unshifted coordinate's
	// derivatives too (the shift is within half a texel)
	const Lighting l = texel_lighting(u, in, texel0);
	// Dynamic lights see the relief
	Lighting lr = l;
	if (repeat && u.relief > 0.0 && rf.glint > 0.0) {
		relief_light(u, rf, l.normal, l.world);
		lr.normal = rf.normal;
	}
	DynamicLight dl;
	if (u.viewer_light.a > 0.0) {
		// The weapon in hand: lit by the lights around the viewer, so the
		// lights at its own (nominal) position are not walked
		dl.amount = u.viewer_light.a;
		dl.tint = u.viewer_light.rgb;
	} else {
		dl = dynamic_light(u, lights, map, occ, lr, repeat);
	}
	dl = add_caustics(u, dl, l);
	const float2 dtx = dfdx(texel0), dty = dfdy(texel0);
	const float2 fw = max(abs(dtx) + abs(dty), float2(1e-4));
	const bool banded = u.shading_style == 0;
	// SHADE_TO_SHADING_TABLE_INDEX, capped at the brightest table; smooth
	// passes through each band's centre. t0: the table without dynamic light
	// Contact shadows (walls and floors only) walk down the ramps too
	auto table = [&](float shade) { return banded ? min(floor(shade * 32.0), 31.0) : clamp(shade * 32.0 - 0.5, 0.0, 31.0); };
	const float4 gi = repeat ? redistribution(u, patches, averages, radiance, l.world)
		: figure_light(u, patches, averages, radiance, l.world);
	const float occlusion = repeat ? contact_shadow(u, casters, l) * gi.a : gi.a;
	const float t = table(classic_shade(u, l.depth, dl.amount, rf.head, rf.glint, rf.fill) * occlusion);
	const float t0 = table(classic_shade(u, l.depth, 0.0, rf.head, rf.glint, rf.fill) * occlusion);
	const float lod = log2(max(max(length(dtx), length(dty)), 1e-6));

	Tap c;
	if ((u.filtering & 1u) == 0) {
		// As the software renderer: the nearest texel
		c = ramp_texel(u, indices, ramps, int2(floor(texel)), size, 0, repeat, t, t0, banded);
	} else if (lod <= 0.0 && max(fw.x, fw.y) > 0.7) {
		// Texels under about 1.4 pixels: blending their edges shows
		// nothing, so the texel alone (a quarter of the work)
		c = ramp_texel(u, indices, ramps, int2(floor(texel)), size, 0, repeat, t, t0, banded);
	} else if (lod <= 0.0) {
		// Magnified: sharp bilinear (see crisp_sample) on shaded texels;
		// inside a texel (most pixels) that is the texel alone
		const float2 p = texel - 0.5;
		const float2 i = floor(p);
		const float2 w = clamp((p - i - 0.5) / fw + 0.5, 0.0, 1.0);
		const int2 b = int2(i);
		if (all(w == floor(w))) {
			c = ramp_texel(u, indices, ramps, b + int2(w), size, 0, repeat, t, t0, banded);
		} else {
			const Tap c00 = ramp_texel(u, indices, ramps, b, size, 0, repeat, t, t0, banded);
			const Tap c10 = ramp_texel(u, indices, ramps, b + int2(1, 0), size, 0, repeat, t, t0, banded);
			const Tap c01 = ramp_texel(u, indices, ramps, b + int2(0, 1), size, 0, repeat, t, t0, banded);
			const Tap c11 = ramp_texel(u, indices, ramps, b + int2(1, 1), size, 0, repeat, t, t0, banded);
			c = mix_taps(mix_taps(c00, c10, w.x), mix_taps(c01, c11, w.x), w.y);
		}
	} else {
		// Minified: the nearest texel of the two closest mip levels of the
		// colour indices (each a Quake-style pick of the colour nearest
		// its 2x2 block's average), blended
		const uint top = indices.get_num_mip_levels() - 1;
		const uint l0 = min(uint(floor(lod)), top);
		const uint l1 = min(l0 + 1, top);
		const float f = (l0 == top) ? 0.0 : lod - floor(lod);
		const Tap c0 = ramp_texel(u, indices, ramps, int2(floor(texel / float(1 << l0))), level_size(size, l0), l0, repeat, t, t0, banded);
		const Tap c1 = ramp_texel(u, indices, ramps, int2(floor(texel / float(1 << l1))), level_size(size, l1), l1, repeat, t, t0, banded);
		c = mix_taps(c0, c1, f);
	}

	const float3 colour = c.c.a > 0.0 ? add_light(c.c0 / c.c.a, c.c.rgb / c.c.a, dl) * gi.rgb : float3(0.0);
	const float3 glow = c.c.a > 0.0 ? c.g / c.c.a : float3(0.0);
	const float f = fog_factor(u, l.fog_distance);
	const float4 out = float4(mix(u.fog_color.rgb, colour, f), u.color.a * c.c.a);
	alpha_test(u, out.a);
	return fogged_frag(u, volume, in, out, glow * f);
}

fragment WorldFrag wall_ramp_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
									  constant Light* lights [[buffer(2)]], device const float4* map [[buffer(3)]],
									  constant Caster* casters [[buffer(4)]], texture3d<float> volume [[texture(3)]],
									  device const Patch* patches [[buffer(5)]], device const float4* averages [[buffer(6)]],
										  texture2d<float> radiance [[texture(4)]], texture2d<uint> indices [[texture(1)]], texture2d<float> ramps [[texture(2)]],
										  device const Occluder* occluders [[buffer(7)]], device const int2* occluder_polygons [[buffer(8)]],
							  device const int* occluder_indices [[buffer(9)]], texture2d_array<float> masks [[texture(6)]],
							  device const float4* surfaces [[buffer(10)]], texture2d_array<float> walls [[texture(7)]])
{
	const Occluders occ = { occluders, occluder_polygons, occluder_indices, masks, u.rampant.y, float2(u.figure_patches.zw),
							u.rampant.z != 0 || (u.rampant.x & 2) != 0 ? surfaces : nullptr, walls };
	return ramp_shade(in, u, lights, map, casters, volume, patches, averages, radiance, indices, ramps, wobbled(u, in), true, occ);
}

fragment WorldFrag sprite_ramp_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
										constant Light* lights [[buffer(2)]], device const float4* map [[buffer(3)]],
										constant Caster* casters [[buffer(4)]], texture3d<float> volume [[texture(3)]],
										device const Patch* patches [[buffer(5)]], device const float4* averages [[buffer(6)]],
										  texture2d<float> radiance [[texture(4)]], texture2d<uint> indices [[texture(1)]], texture2d<float> ramps [[texture(2)]],
										  device const Occluder* occluders [[buffer(7)]], device const int2* occluder_polygons [[buffer(8)]],
							  device const int* occluder_indices [[buffer(9)]], texture2d_array<float> masks [[texture(6)]],
							  device const float4* surfaces [[buffer(10)]], texture2d_array<float> walls [[texture(7)]])
{
	const Occluders occ = { occluders, occluder_polygons, occluder_indices, masks, u.rampant.y, float2(u.figure_patches.zw),
							u.rampant.z != 0 || (u.rampant.x & 2) != 0 ? surfaces : nullptr, walls };
	return ramp_shade(in, u, lights, map, casters, volume, patches, averages, radiance, indices, ramps, in.texcoord, false, occ);
}

// Character shadows (Round 12): the sprite's quad projected onto its
// floor along a light direction, drawn before the sprite as a dark
// silhouette from the sprite's own alpha (a coarser mip softens the
// edge), depth tested so walls cut it. color.a is the strength.
fragment WorldFrag shadow_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
								   texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]])
{
	const float a = tex.sample(smp, in.texcoord, level(1.5)).a;
	alpha_test(u, a);
	WorldFrag o;
	o.color = float4(0.0, 0.0, 0.0, a * u.color.a);
	o.glow = float4(0.0);
	o.distance = float4(in.fog_distance, 0.0, 0.0, 0.0);	// the floor's own distance stays
	return o;
}

// Liquid surfaces (W1). Drawn after everything beneath them and before
// anything on the viewer's side, they read what is already drawn at this
// pixel (Apple GPUs keep the frame in tile memory): its colour, glow and
// distance. The view's path under the surface (that distance minus the
// surface's own) gives per-channel absorption and murk, so the floor shows
// through shallow water and fades into the liquid's colour with depth.
// The surface ripples (animated normals: its texture shimmers, and dynamic
// lights and the headlight glint on it, stronger at grazing angles). Lava
// is opaque and glows. Refraction of what is below would need a copy of
// the frame, which a single pass does not have.
// Reflecting liquids (R2, Rampant): a ray from the surface walked through
// the map as the light shadows are (exact in 5D space) to the first floor,
// ceiling or wall it meets, which is shaded from the surface table
// (DurandalSurfaces.h): its wall art, placed as the rasteriser places it,
// at its own light with the headlight by its distance from the viewer, as
// Marathon lights it. Landscape surfaces show the sky by direction.
struct SurfaceHit {
	int poly;
	int part;		// 0 floor, 1 ceiling, 2 + 2e edge e's upper part, 3 + 2e its lower part
	int edge;
	float t;
	float3 at;
};

static bool trace_surfaces(device const float4* map, int poly, float3 o, float3 d, float far, thread SurfaceHit& hit)
{
	float t_prev = 0.0;
	for (int step = 0; step < 48; ++step) {
		if (poly < 0)
			return false;
		const float4 h = map[poly * 9];
		const int n = int(h.x);
		float leave = 1e9;
		int next = -1, edge = -1;
		for (int i = 0; i < n; ++i) {
			const float4 e0 = map[poly * 9 + 1 + i];
			const float4 e1 = map[poly * 9 + 1 + (i + 1 == n ? 0 : i + 1)];
			const float2 e = e1.xy - e0.xy;
			const float den = d.x * e.y - d.y * e.x;
			if (abs(den) < 1e-6)
				continue;
			const float2 w = e0.xy - o.xy;
			const float t = (w.x * e.y - w.y * e.x) / den;
			const float sp = (w.x * d.y - w.y * d.x) / den;
			if (t > t_prev + 1e-3 && t < leave && sp >= -1e-3 && sp <= 1.0 + 1e-3) {
				leave = t;
				next = int(e0.z);
				edge = i;
			}
		}
		float t_plane = 1e9;
		int plane = -1;
		if (d.z < -1e-5) { t_plane = (h.y - o.z) / d.z; plane = 0; }
		else if (d.z > 1e-5) { t_plane = (h.z - o.z) / d.z; plane = 1; }
		if (plane >= 0 && t_plane <= leave) {
			if (t_plane > far)
				return false;
			hit.poly = poly; hit.part = plane; hit.edge = -1; hit.t = t_plane; hit.at = o + d * t_plane;
			return true;
		}
		if (leave > far || edge < 0)
			return false;
		const float z = o.z + d.z * leave;
		bool wall = next < 0;
		bool upper = true;
		if (!wall) {
			const float4 hn = map[next * 9];
			wall = z < hn.y || z > hn.z;
			upper = z > hn.z;
		}
		if (wall) {
			hit.poly = poly; hit.part = 2 + 2 * edge + (upper ? 0 : 1); hit.edge = edge; hit.t = leave;
			hit.at = o + d * leave;
			return true;
		}
		poly = next;
		t_prev = leave;
	}
	return false;
}

static float3 sky_colour(float4 mapping, texture2d<float> sky, float3 d)
{
	const float turn = atan2(d.y, d.x) * 0.15915494;
	const float elevation = asin(clamp(d.z * rsqrt(max(length_squared(d), 1e-9)), -1.0, 1.0));
	const float2 uv = float2(mapping.x * (turn + mapping.y), clamp(0.5 - elevation / max(mapping.z, 1e-3), 0.0, 1.0));
	constexpr sampler s(filter::linear, mip_filter::linear, address::repeat);
	return sky.sample(s, uv, level(1.0)).rgb;
}

static float3 shade_hit(constant Uniforms& u, device const float4* surfaces, device const float4* map,
						texture2d_array<float> walls, texture2d<float> sky, thread const SurfaceHit& hit, float3 d,
						float travelled)
{
	const float4 e = surfaces[1 + hit.poly * kSurfacesPerPolygon + hit.part];
	if (e.x < -1.5)
		return sky_colour(surfaces[0], sky, d);
	float3 colour = float3(0.5);
	if (e.x > -0.5) {
		// The colour array holds the art as it lies on the surface: x across
		// the wall (or the floor's y), y down it (or the floor's x)
		float2 uv;
		if (hit.part < 2) {
			uv = float2(hit.at.y + e.z, hit.at.x + e.y) / 1024.0;
		} else {
			const float2 v0 = map[hit.poly * 9 + 1 + hit.edge].xy;
			uv = float2(e.y + length(hit.at.xy - v0), e.z - hit.at.z) / 1024.0;
		}
		constexpr sampler s(filter::linear, mip_filter::linear, address::repeat);
		colour = walls.sample(s, uv, uint(e.x), level(clamp(log2(max(travelled, 1.0) / 2048.0), 0.0, 5.0))).rgb;
	}
	// As classic_intensity: the surface's light and the headlight by the
	// hit's distance from the viewer
	const float depth = length(hit.at - u.camera.xyz) / 8192.0;
	const float ml = clamp(u.self_luminosity + u.flare - depth, 0.0, 1.0);
	const float light = min(e.w, 1.0);
	const float intensity = light > ml ? light + ml * 0.5 : light * 0.5 + ml;
	return colour * clamp(intensity, 0.0, 1.0);
}

struct LiquidFrag {
	float4 color [[color(0)]];
	float4 glow [[color(1)]];
	float4 distance [[color(2)]];	// Round 12: the far value, as the sky: no ambient shadows on liquids
};

static float2 wave_slope(float2 p, float t, float strength)
{
	float2 g = float2(0.0);
	const float2 k[4] = { float2(1.0, 0.3), float2(-0.4, 1.1), float2(0.7, -0.9), float2(-1.3, -0.5) };
	const float w[4] = { 0.9, 1.3, 0.7, 1.6 };
	for (int i = 0; i < 4; ++i) {
		const float2 dir = k[i] * (1.0 + 0.6 * float(i));
		g += dir * cos(dot(dir, p) + w[i] * t) / (1.0 + float(i));
	}
	return g * 0.08 * strength;
}

// Living water (R2): rings spreading from anything wading and from
// splashes (DurandalLights::GatherRipples; sources[0].x is the count)
static float2 ripple_slope(constant float4* sources, float2 p, float t)
{
	float2 g = float2(0.0);
	const int count = int(sources[0].x);
	for (int i = 1; i <= count; ++i) {
		const float2 d = p - sources[i].xy;
		const float r = length(d) / 1024.0;
		if (r > 2.5 || r < 1e-3)
			continue;
		const float ring = cos(r * 21.0 - t * 7.0) * exp(-r / 0.7) * smoothstep(0.0, 0.08, r);
		g += (d / (r * 1024.0)) * ring * sources[i].z * 0.12;
	}
	return g;
}

// A figure in a reflection (R2): the nearest figure card about polygon
// `poly` the reflected ray from `from` along `d` meets before t = 1,
// as its colour (rgb) and whether one was met (a)
static float4 reflected_figure(device const Occluder* occluders, device const int2* lists, device const int* indices,
							   texture2d_array<float> masks, int polygon_count, int poly, float3 from, float3 d)
{
	if (poly < 0 || poly >= polygon_count)
		return float4(0.0);
	const int2 range = lists[poly];
	const float dd = dot(d.xy, d.xy);
	if (range.y == 0 || dd < 1.0)
		return float4(0.0);
	const float2 across = float2(d.y, -d.x) * rsqrt(dd);
	constexpr sampler s(filter::linear, mip_filter::linear, address::clamp_to_zero);
	float best = 1.0;
	float4 found = float4(0.0);
	for (int k = 0; k < range.y; ++k) {
		const Occluder o = occluders[indices[range.x + k]];
		const float t = dot(o.position.xy - from.xy, d.xy) / dd;
		if (t <= 0.0 || t >= best)
			continue;
		const float3 p = from + d * t;
		const float h = dot(p.xy - o.position.xy, across);
		const float z = p.z - o.position.z;
		if (h < o.extent.x || h > o.extent.y || z < o.extent.z || z > o.extent.w)
			continue;
		float u = (h - o.extent.x) / (o.extent.y - o.extent.x);
		if (o.info.y != 0)
			u = 1.0 - u;
		const float v = (o.extent.w - z) / (o.extent.w - o.extent.z);
		const float4 c = masks.sample(s, float2(u * o.mask.x, v * o.mask.y), uint(o.info.x), level(1.0));
		if (c.a < 0.5)
			continue;
		best = t;
		found = float4(c.rgb * (float(o.info.z) / 1000.0), 1.0);
	}
	return found;
}

fragment LiquidFrag liquid_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
									constant Light* lights [[buffer(2)]], device const float4* map [[buffer(3)]],
									texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]],
									texture3d<float> volume [[texture(3)]],
									float4 below [[color(0)]], float4 below_glow [[color(1)]],
									float4 below_dist [[color(2)]],
									device const float4* surfaces [[buffer(10)]],
									texture2d_array<float> walls [[texture(7)]], texture2d<float> sky [[texture(8)]],
									constant float4* ripples [[buffer(11)]],
									device const Occluder* occluders [[buffer(7)]], device const int2* occluder_polygons [[buffer(8)]],
									device const int* occluder_indices [[buffer(9)]], texture2d_array<float> masks [[texture(6)]])
{
	const float below_distance = below_dist.r;	// the same type as WorldFrag writes
	const float t = u.time / 30.0;
	const float opacity = u.liquid.z, waves = u.liquid.w;
	const float3 camera = u.camera.xyz;
	const float3 v = normalize(in.world - camera);

	// Rippling normal (seen from below, the surface faces down); with living
	// water (R2) rings spread from anything wading and from splashes
	float2 slope = wave_slope(in.world.xy / 1024.0, t, waves);
	if (u.rampant.z != 0)
		slope += ripple_slope(ripples, in.world.xy, t) * max(waves, 0.3);
	float3 n = normalize(float3(-slope, 1.0));
	if (camera.z < in.world.z)
		n = float3(n.xy, -n.z);

	// The surface: its texture, shimmering with the ripples, lit as a floor.
	// Kept cheap: with smooth edges (MSAA) a shader that reads the frame runs
	// per sample, so no texel-centre snapping and no light shadows here
	const float2 tc = wobbled(u, in) + slope * 0.15;
	const Lighting l = { in.classic_depth, in.fog_distance, in.world, n };
	const DynamicLight dl = dynamic_light(u, lights, map, l, false, false);
	const float4 texel = crisp_sample(u, tex, smp, tc);
	const float3 intensity = add_light(classic_intensity(u, l.depth), classic_intensity(u, l.depth, dl.amount), dl);
	// Volumetric fog to the surface; what is below already has its own
	const float4 haze = fog_at(u, volume, in.position.xy, in.fog_distance);
	const float3 surface = texel.rgb * intensity * haze.a + haze.rgb;

	// What is below: the light reaching it falls off with its depth under
	// the surface (so the floor darkens as the liquid deepens), then the
	// view's path through the liquid dims and tints it; murk takes on the
	// surface's own lit colour
	const float3 absorb = u.liquid_colour.rgb / max(u.liquid.y, 1.0);
	// With exact distances below (R3) the surface's own is exact too
	const float surface_distance = u.rampant.w != 0 ? length(in.world - camera) : in.fog_distance;
	const float path = max(below_distance - surface_distance, 0.0);
	float depth = 0.0;
	if (camera.z > in.world.z)
		depth = max(in.world.z - (camera.z + v.z * below_distance), 0.0);
	const float3 reach = exp(-depth * absorb * 1.5);
	const float3 transmit = exp(-path * absorb);
	// Refraction (R2): the floor beneath is bent by the ripples. What is
	// below is already drawn exactly; the texture where the bent ray lands
	// against where the straight one does is carried onto it as a ratio,
	// so the floor's pattern wavers while its light stays as drawn
	float3 below_rgb = below.rgb;
	if (u.rampant.z != 0 && camera.z > in.world.z && opacity < 0.99 && u.polygon >= 0) {
		const float3 straight = camera + v * below_distance;
		const float4 floor_here = surfaces[1 + u.polygon * kSurfacesPerPolygon];
		if (floor_here.x > -0.5 && abs(straight.z - map[u.polygon * 9].y) < 24.0) {
			const float3 bent = refract(v, n, 1.0 / 1.33);
			SurfaceHit hit;
			if (length_squared(bent) > 0.0 && trace_surfaces(map, u.polygon, in.world - float3(0.0, 0.0, 2.0), bent, 16384.0, hit) &&
				hit.part == 0) {
				const float4 floor_there = surfaces[1 + hit.poly * kSurfacesPerPolygon];
				if (floor_there.x > -0.5) {
					constexpr sampler fs(filter::linear, mip_filter::linear, address::repeat);
					const float3 was = walls.sample(fs, float2(straight.y + floor_here.z, straight.x + floor_here.y) / 1024.0,
													uint(floor_here.x), level(1.0)).rgb;
					const float3 now = walls.sample(fs, float2(hit.at.y + floor_there.z, hit.at.x + floor_there.y) / 1024.0,
													uint(floor_there.x), level(1.0)).rgb;
					below_rgb *= clamp((now + 0.03) / (was + 0.03), float3(0.5), float3(2.0));
				}
			}
		}
	}
	const float3 seen = below_rgb * reach * transmit + surface * (1.0 - transmit);

	// Reflection: stronger at grazing angles (Fresnel), plus glints from
	// the dynamic lights and the headlight
	const float facing = saturate(dot(n, -v));
	const float fresnel = 0.02 + 0.98 * pow(1.0 - facing, 5.0);
	float3 glint = float3(0.0);
	for (uint m = u.culling.x & ((1u << u.light_count) - 1u); m != 0u; m &= m - 1u) {
		const uint i = ctz(m);
		const float3 to = lights[i].position_radius.xyz - in.world;
		const float r = lights[i].position_radius.w;
		const float d2 = length_squared(to);
		if (d2 >= r * r)
			continue;
		const float spec = pow(saturate(dot(reflect(v, n), normalize(to))), 60.0);
		glint += lights[i].colour_strength.rgb * lights[i].colour_strength.w * spec * (1.0 - d2 / (r * r));
	}
	const float head = pow(saturate(dot(reflect(v, n), -v)), 40.0) * saturate(u.self_luminosity + u.flare - l.depth);
	const float3 sheen = surface * fresnel + (glint + head * 0.25) * haze.a;

	float3 colour = mix(seen, surface, opacity) + sheen * (1.0 - opacity * 0.5);
	// Reflecting liquids (R2): the room above and the sky, broken by the
	// ripples, in place of the surface's own colour in the sheen
	if (u.rampant.z != 0 && camera.z > in.world.z && opacity < 0.99) {
		// A steep ripple at a grazing view can turn the ray under the
		// surface; it stays just above it
		float3 r = reflect(v, n);
		r = normalize(float3(r.xy, max(r.z, 0.02)));
		SurfaceHit hit;
		float3 mirror = surface;	// nothing within reach: the surface's own colour, as before
		const bool met = trace_surfaces(map, u.polygon, in.world + float3(0.0, 0.0, 2.0), r, 16384.0, hit);
		if (met) {
			mirror = shade_hit(u, surfaces, map, walls, sky, hit, r, in.fog_distance + hit.t);
			mirror = mix(u.fog_color.rgb, mirror, fog_factor(u, hit.t)) * haze.a + haze.rgb;
		}
		// Figures standing by the water, in front of what the ray met
		const float reach = met ? hit.t : 8192.0;
		const float4 figure = reflected_figure(occluders, occluder_polygons, occluder_indices, masks, u.rampant.y,
											   u.polygon, in.world + float3(0.0, 0.0, 2.0), r * reach);
		if (figure.a > 0.5) {
			const float depth_h = length(in.world - camera) / 8192.0;
			const float ml = clamp(u.self_luminosity + u.flare - depth_h, 0.0, 1.0);
			mirror = figure.rgb * clamp(1.0 + ml * 0.5, 0.0, 1.5) * haze.a + haze.rgb;
		}
		// A quarter less than full Fresnel (the owner, 2 Oct 2026: slightly
		// less reflective; the ripples and glints unchanged)
		const float weight = fresnel * (1.0 - opacity * 0.5) * 0.75;
		colour = mix(mix(seen, surface, opacity), mirror, weight) + (glint + head * 0.25) * haze.a * (1.0 - opacity * 0.5);
	}
	const float f = fog_factor(u, l.fog_distance);
	colour = mix(u.fog_color.rgb, colour, f);

	LiquidFrag o;
	o.color = float4(colour, 1.0);
	// Glow below shows through as its light does; lava glows itself
	const float3 own_glow = u.write_glow ? texel.rgb * u.liquid_colour.w * f : float3(0.0);
	o.glow = float4(below_glow.rgb * reach * transmit * (1.0 - opacity) + own_glow, 1.0);
	o.distance = float4(1.0e9, 0.0, 0.0, 1.0);	// Round 12: far, as the sky: exempt from ambient shadows and distance shade
	return o;
}

// Air that moves (Rampant; DurandalAir.h): dust motes and embers, each a
// small soft dot facing the viewer, composed over what is drawn and hidden
// where a surface is nearer (the distance image, read in tile memory)
struct Mote {
	float4 position_size;
	float4 colour;
	float4 info;
};

struct MoteOut {
	float4 position [[position]];
	float2 corner;
	float4 colour;
	float glow;
	float distance;
};

vertex MoteOut mote_vertex(uint vid [[vertex_id]], uint iid [[instance_id]], constant Uniforms& u [[buffer(1)]],
						   device const Mote* motes [[buffer(12)]])
{
	const Mote m = motes[iid];
	MoteOut o;
	o.corner = float2((vid & 1u) ? 1.0 : -1.0, (vid & 2u) ? 1.0 : -1.0);
	float4 eye = u.modelview * float4(m.position_size.xyz, 1.0);
	o.distance = length(eye.xyz);
	eye.xy += o.corner * m.position_size.w;
	const float4 clip = u.projection * eye;
	o.position = float4(clip.x, clip.y, (clip.z + clip.w) * 0.5, clip.w);	// as world_vertex: GL depth to Metal's
	o.colour = m.colour;
	o.glow = m.info.x;
	return o;
}

struct MoteFrag {
	float4 color [[color(0)]];
	float4 glow [[color(1)]];
	float4 distance [[color(2)]];
};

fragment MoteFrag mote_fragment(MoteOut in [[stage_in]], constant Uniforms& u [[buffer(1)]],
								float4 below [[color(0)]], float4 below_glow [[color(1)]], float4 below_dist [[color(2)]])
{
	if (in.distance > below_dist.r)
		discard_fragment();	// behind a wall, floor or figure
	const float fall = saturate(1.0 - length_squared(in.corner));
	const float a = in.colour.a * fall * fall * fog_factor(u, in.distance);
	MoteFrag o;
	o.color = float4(below.rgb + in.colour.rgb * a, below.a);
	o.glow = float4(below_glow.rgb + (u.write_glow ? in.colour.rgb * a * in.glow : float3(0.0)), below_glow.a);
	o.distance = below_dist;
	return o;
}

static float static_rand(float2 co)
{
	const float dt = dot(co, float2(12.9898, 78.233));
	const float sn = fmod(dt, 3.14);
	return fract(sin(sn) * 43758.5453);
}

fragment WorldFrag invincible_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
									texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]])
{
	const float block = u.static_block;
	const float moment = fract(u.time / 10000.0);
	const float2 entropy = moment * round(in.position.xy / block);
	const float sr = static_rand(entropy);
	const float sg = static_rand(entropy * sr);
	const float sb = static_rand(entropy * sg);
	const float3 intensity = float3(sr, sg, sb);
	float4 color = tex.sample(smp, in.texcoord);
	float drop = max(max(intensity.r, intensity.g), intensity.b);
	drop = drop * drop * drop;
	if (drop < u.transfer_fade_out)
		color.a = 0.0;
	const float f = fog_factor(u, in.fog_distance);
	const float4 out = float4(mix(u.fog_color.rgb, intensity, f), u.color.a * color.a);
	alpha_test(u, out.a);
	return world_frag(in, out);
}

fragment WorldFrag invisible_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
								   texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]])
{
	const float4 color = tex.sample(smp, in.texcoord);
	const float f = fog_factor(u, in.fog_distance);
	const float4 out = float4(mix(u.fog_color.rgb, float3(0.0), f), u.color.a * color.a * u.visibility);
	alpha_test(u, out.a);
	return world_frag(in, out);
}

// True Look: a flat landscape follows the view's yaw only. The eye space
// also holds the view's pitch and roll (Rasterizer_Metal builds the view as
// the base axes, roll about the forward axis, pitch about the side axis,
// then yaw), so they are taken off here; mapped from the eye space as it
// came, the sky turned and leaned with the head instead of staying with the
// world (the owner, 2 Oct 2026: the skies "sway"). Pitch and roll are 0
// without True Look, where nothing changes
static float3 level_eye(constant Uniforms& u, float3 rel)
{
	float3 v = float3(-rel.z, rel.x, rel.y);	// the view's axes: forward, side, up
	const float cr = cos(u.roll), sr = sin(u.roll);
	v = float3(v.x, cr * v.y + sr * v.z, -sr * v.y + cr * v.z);	// the roll taken off
	const float cp = cos(u.pitch), sp = sin(u.pitch);
	v = float3(cp * v.x - sp * v.z, v.y, sp * v.x + cp * v.z);		// the pitch taken off
	return float3(v.y, v.z, -v.x);
}

static float2 landscape_uv(constant Uniforms& u, float3 eye)
{
	const float zoom = 1.2;
	const float3 rel = level_eye(u, eye);
	// As the OpenGL renderer the sky is flat on the screen, so it stretches
	// towards the sides at wide fields of view; with HDR Sky (V2) it is a
	// cylinder around the viewer, identical at the centre of the view. Ahead
	// is -z: the cylinder is written with atan2 and the horizontal length so
	// it runs on, unbroken, up to and past straight up
	const bool cylinder = (u.sky & 1u) != 0;
	const float across = cylinder ? atan2(-rel.x, -rel.z) : rel.x / rel.z;
	const float up = cylinder ? -rel.y * rsqrt(max(rel.x * rel.x + rel.z * rel.z, 1e-12)) : rel.y / rel.z;
	const float x = across / zoom + atan2(cos(u.yaw), sin(u.yaw));
	const float y = up / zoom;
	return float2(u.offsetx - x * u.scalex, u.offsety - y * u.scaley);
}

// A landscape without vertical repeat ends at its image's top and bottom:
// the sampler repeats it mirrored (as OpenGL did), and the image proper is
// the one period of that around the horizon (v = offsety; for M2's default
// options, -1 to 0, about 45 degrees either side of the horizon). True Look
// sees past it (to straight up, where the cylinder's height runs to
// infinity), and the mirrored copies came round again and again there (the
// owner: the skies "wrap"). Beyond the image the sky fades to the average of
// its edge row, so stars do not smear into streaks
static float4 landscape_colour(constant Uniforms& u, texture2d<float> tex, sampler smp, float3 eye)
{
	const float2 uv = landscape_uv(u, eye);
	if ((u.sky & 4u) == 0)
		return tex.sample(smp, uv);
	const float top = floor(u.offsety), edge = 0.5 / float(tex.get_height());
	const float v = clamp(uv.y, top + edge, top + 1.0 - edge);
	const float4 c = tex.sample(smp, float2(uv.x, v), level(0));
	const float over = max(top + edge - uv.y, uv.y - (top + 1.0 - edge));
	if (over <= 0.0)
		return c;
	float4 row = float4(0.0);
	for (int i = 0; i < 16; ++i)
		row += tex.sample(smp, float2((float(i) + 0.5) / 16.0, v), level(0));
	return mix(c, row / 16.0, smoothstep(0.0, 0.12, over));
}

// HDR sky (V2): the brightest parts of the landscape (sunlit sky, fiery
// clouds, moons) glow, so they sit above SDR white and bloom a little
static float3 sky_glow(constant Uniforms& u, float3 c)
{
	if (!u.write_glow || (u.sky & 2u) == 0)
		return float3(0.0);
	const float luminance = dot(c, float3(0.2126, 0.7152, 0.0722));
	// A whole bright sky must not wash the view out: only near-white
	// highlights glow fully
	return c * smoothstep(0.75, 1.0, luminance) * 0.5;
}

static float2 landscape_sphere_uv(constant Uniforms& u, float3 rel)
{
	const float cy = cos(u.yaw), sy = sin(u.yaw);
	const float cp = cos(-u.pitch), sp = sin(-u.pitch);
	const float cr = cos(-u.roll), sr = sin(-u.roll);
	const float ca = cos(u.offsetx), sa = sin(u.offsetx);
	const float ce = cos(-u.offsety), se = sin(-u.offsety);
	// GLSL mat3(...) takes columns; these match landscape_sphere.frag.
	// True Look: the view's roll (about the eye's z axis) is taken off
	// first, the same way as the pitch, so the sky stays put in the world
	const float3x3 rotate_yaw = float3x3(float3(cy, 0, sy), float3(0, 1, 0), float3(-sy, 0, cy));
	const float3x3 rotate_pitch = float3x3(float3(1, 0, 0), float3(0, cp, -sp), float3(0, sp, cp));
	const float3x3 rotate_roll = float3x3(float3(cr, -sr, 0), float3(sr, cr, 0), float3(0, 0, 1));
	const float3x3 rotate_azimuth = float3x3(float3(ca, 0, sa), float3(0, 1, 0), float3(-sa, 0, ca));
	const float3x3 rotate_elevation = float3x3(float3(1, 0, 0), float3(0, ce, -se), float3(0, se, ce));
	const float3 d = rotate_azimuth * rotate_elevation * rotate_yaw * rotate_pitch * rotate_roll * normalize(rel);
	const float pi = 3.14156;
	const float theta = atan2(d.x, d.z);
	const float phi = acos(d.y);
	return float2((pi - theta) / (2.0 * pi), phi / pi);
}

fragment WorldFrag landscape_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
								   texture3d<float> volume [[texture(3)]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]])
{
	const float4 color = landscape_colour(u, tex, smp, in.rel_dir);
	return fogged_frag(u, volume, in, float4(mix(color.rgb, u.fog_color.rgb, u.fog_mix), 1.0), sky_glow(u, color.rgb) * (1.0 - u.fog_mix));
}

fragment WorldFrag landscape_infravision_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
											   texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]])
{
	const float4 color = landscape_colour(u, tex, smp, in.rel_dir);
	const float avg = (color.r + color.g + color.b) / 3.0;
	return world_frag(in, float4(mix(u.color.rgb * avg, u.fog_color.rgb, u.fog_mix), 1.0), float3(0.0), u.distance_mode);
}

fragment WorldFrag landscape_sphere_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
										  texture3d<float> volume [[texture(3)]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]])
{
	const float4 color = tex.sample(smp, landscape_sphere_uv(u, in.rel_dir));
	return fogged_frag(u, volume, in, float4(mix(color.rgb, u.fog_color.rgb, u.fog_mix), 1.0), sky_glow(u, color.rgb) * (1.0 - u.fog_mix));
}

fragment WorldFrag landscape_sphere_infravision_fragment(WorldIn in [[stage_in]], constant Uniforms& u [[buffer(1)]],
													  texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]])
{
	const float4 color = tex.sample(smp, landscape_sphere_uv(u, in.rel_dir));
	const float avg = (color.r + color.g + color.b) / 3.0;
	return world_frag(in, float4(mix(u.color.rgb * avg, u.fog_color.rgb, u.fog_mix), 1.0), float3(0.0), u.distance_mode);
}

// Volumetric fog (V1). One thread per column of the fog volume (a cell of
// kVolumeCell x kVolumeCell pixels) follows its view ray through the map's
// polygons, as Marathon's own line-of-sight tests do (so 5D space works),
// and integrates the haze along it: density from the scene's base
// (thicker near the floor, stirred by drifting dust), lit by the room's
// own light where it passes (so dark rooms keep dark air) and by the
// dynamic lights near it that can reach it, and lava below it. Under a
// liquid the viewer is in, the murk instead. Each slice stores the in-scattered light and the
// transmittance from the viewer to its far end; world fragments read them
// at their own distance (fog_at). Stops where the ray meets a wall, step
// or ceiling: nothing beyond it can be seen.
struct VolumeParams {
	float4x4 inverse;
	float4 camera;
	float4 air;
	float4 murk;
	uint4 grid;
	int polygon;
	uint viewer_under;
	uint shadows;
	float time;
	float nearest;
	float slice_scale;
	float dust;
	float light_scale;
	float mist;
	int figures;		// shafts (R1): polygons in the occluder lists, 0 none
	float pad[2];
};

static float fog_hash(float3 p)
{
	p = fract(p * 0.3183099 + 0.1);
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

static float fog_noise(float3 x)
{
	const float3 i = floor(x);
	float3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(fog_hash(i), fog_hash(i + float3(1, 0, 0)), f.x),
				   mix(fog_hash(i + float3(0, 1, 0)), fog_hash(i + float3(1, 1, 0)), f.x), f.y),
			   mix(mix(fog_hash(i + float3(0, 0, 1)), fog_hash(i + float3(1, 0, 1)), f.x),
				   mix(fog_hash(i + float3(0, 1, 1)), fog_hash(i + float3(1, 1, 1)), f.x), f.y), f.z);
}

// Where a ray (origin a, direction d per unit distance, in the plane)
// leaves polygon `poly` after distance `after`, and the polygon across
static float polygon_exit(device const float4* map, int poly, float2 a, float2 d, float after, thread int& next,
						  thread int& edge)
{
	const int n = int(map[poly * 9].x);
	float best = 1e9;
	next = -1;
	edge = -1;
	for (int i = 0; i < n; ++i) {
		const float4 e0 = map[poly * 9 + 1 + i];
		const float4 e1 = map[poly * 9 + 1 + (i + 1 == n ? 0 : i + 1)];
		const float2 e = e1.xy - e0.xy;
		const float den = d.x * e.y - d.y * e.x;
		if (abs(den) < 1e-6)
			continue;
		const float2 w = e0.xy - a;
		const float t = (w.x * e.y - w.y * e.x) / den;
		const float s = (w.x * d.y - w.y * d.x) / den;
		if (t > after + 1e-3 && t < best && s >= -1e-3 && s <= 1.0 + 1e-3) {
			best = t;
			next = int(e0.z);
			edge = i;
		}
	}
	return best;
}

kernel void volume_kernel(uint2 gid [[thread_position_in_grid]], constant VolumeParams& p [[buffer(0)]],
						  constant Light* lights [[buffer(1)]], device const float4* map [[buffer(2)]],
						  texture3d<half, access::write> out [[texture(0)]],
						  device const Occluder* occluders [[buffer(3)]], device const int2* occluder_polygons [[buffer(4)]],
						  device const int* occluder_indices [[buffer(5)]], texture2d_array<float> masks [[texture(1)]])
{
	// Shafts (R1, Rampant): figures near a light shade the haze behind them
	const Occluders occ = { occluders, occluder_polygons, occluder_indices, masks, p.figures, float2(1e9), nullptr, masks };
	if (gid.x >= p.grid.x || gid.y >= p.grid.y)
		return;
	const float2 uv = (float2(gid) + 0.5) / float2(p.grid.xy);
	const float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
	const float4 a = p.inverse * float4(ndc, -1.0, 1.0);
	const float4 b = p.inverse * float4(ndc, 1.0, 1.0);
	const float3 o = p.camera.xyz;
	const float3 dir = normalize(b.xyz / b.w - a.xyz / a.w);

	int poly = p.polygon;
	bool open = poly >= 0;
	int next = -1, edge = -1;
	float leave = open ? polygon_exit(map, poly, o.xy, dir.xy, 0.0, next, edge) : 0.0;
	int crossings = 0;
	float3 scatter = float3(0.0);
	float trans = 1.0;
	float previous = 0.0;
	const float3 drift = float3(0.22, 0.14, 0.06) * p.time;
	uint known = 0u, reaches = 0u;	// dynamic lights' reach into the current polygon
	for (uint k = 0; k < p.grid.z; ++k) {
		const float d = p.nearest * exp(float(k) / p.slice_scale);
		const float m = 0.5 * (previous + d);
		// Into the polygon the middle of this slice is in
		while (open && m > leave) {
			if (next < 0 || ++crossings > 64) {
				open = false;
				break;
			}
			const float z = o.z + dir.z * leave;
			const float4 h = map[poly * 9], hn = map[next * 9];
			if (z < max(h.y, hn.y) - 1.0 || z > min(h.z, hn.z) + 1.0) {
				open = false;	// a wall, step or lower ceiling
				break;
			}
			poly = next;
			leave = polygon_exit(map, poly, o.xy, dir.xy, leave, next, edge);
			known = 0u;
			reaches = 0u;
		}
		if (open) {
			const float3 x = o + dir * m;
			const float4 h = map[poly * 9];
			const float lit = mix(h.w, map[poly * 9 + 1].w, saturate((x.z - h.y) / max(h.z - h.y, 1.0)));
			const bool under = x.z < map[poly * 9 + 2].w;
			float sigma = 0.0;
			float3 colour = float3(0.0);
			if (!under) {
				// Drifting patches and wisps: layered noise, thresholded so
				// there are clumps and clearer gaps; smoothed out where a
				// slice is deeper than the patches are (far away)
				const float3 q = x / 700.0 + drift;
				const float fbm = 0.55 * fog_noise(q) + 0.3 * fog_noise(q * 2.13 + 5.2) + 0.15 * fog_noise(q * 4.37 + 1.7);
				const float clumps = 0.1 + 2.6 * smoothstep(0.38, 0.7, fbm);
				const float detail = p.dust * saturate(2048.0 / max(d - previous, 1.0));
				sigma = p.air.w * mix(1.0, clumps, detail) * (1.0 + p.mist * exp(-max(x.z - h.y, 0.0) / 500.0));
				colour = p.air.rgb * lit;
				// Smoke rising off lava, for its glow to show in
				if (map[poly * 9 + 3].w == 2.0)
					sigma += p.air.w * 4.0 * clumps * exp(-max(x.z - map[poly * 9 + 2].w, 0.0) / 700.0);
			} else if (p.viewer_under) {
				// Seen from above, liquids draw their own depth (liquid_fragment)
				sigma = p.murk.w;
				colour = p.murk.rgb * (0.3 + 0.7 * lit);
			}
			if (sigma > 0.0) {
				float3 lamp = float3(0.0);
				// Lava lights the air just above it
				if (!under && map[poly * 9 + 3].w == 2.0)
					lamp += float3(1.0, 0.42, 0.12) * 0.9 * exp(-(x.z - map[poly * 9 + 2].w) / 700.0);
				for (uint i = 0; i < p.grid.w; ++i) {
					const float3 to = lights[i].position_radius.xyz - x;
					const float r = lights[i].position_radius.w * 1.25;
					const float d2 = length_squared(to);
					if (d2 >= r * r)
						continue;
					float f = 1.0 - d2 / (r * r);
					f *= f;
					// Whether the light reaches this polygon, checked once per
					// polygon the ray passes through (bits: known, reaches)
					if (p.shadows) {
						const uint bit = 1u << i;
						if (!(known & bit)) {
							known |= bit;
							if (light_reaches(map, poly, x, lights[i].position_radius.xyz, lights[i].info.x))
								reaches |= bit;
						}
						if (!(reaches & bit))
							continue;
					}
					// The figures standing about this polygon, for the lights that
					// throw their shadows (the nearest few, where strong)
					float seen = 1.0;
					if (p.figures > 0 && m < 8192.0 && lights[i].info.z != 0 && lights[i].colour_strength.w * f >= 0.1) {
						const float size = lights[i].info.y > 0 ? float(lights[i].info.y) : 150.0;
						seen = figures_between(occ, poly, x, lights[i].position_radius.xyz - x, 0.0, 1.0, size);
					}
					lamp += lights[i].colour_strength.rgb * lights[i].colour_strength.w * f * seen;
				}
				const float e = exp(-sigma * (d - previous));
				scatter += trans * (colour + lamp * p.light_scale) * (1.0 - e);
				trans *= e;
			}
		}
		out.write(half4(half3(scatter), half(trans)), uint3(gid, k));
		previous = d;
	}
}

// Light redistribution (E4). Marathon gives each surface a brightness;
// modern light may only move light around within it (AUDIT 3.6). The
// surface cache holds, per lumel, the light arriving there: rays traced
// through the map's polygons (walking them as Marathon's line-of-sight
// tests do, so 5D space works) see what every surface gives off, which is
// simply its authored brightness times its texture's colour (sky through
// landscape surfaces, reviewed wall lights brighter). Hits close by count
// for less, as they shut out the rest of the room: corners, ledges and the
// feet of walls gather less. Baked progressively (a running average) for
// the surfaces in view; each patch's average is kept alongside. Surfaces
// are then shaded with their own brightness x (lumel / patch average),
// clamped, so every surface keeps its designed average.

static float bake_hash(uint3 v)
{
	v = v * 1664525u + 1013904223u;
	v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
	v ^= v >> 16u;
	v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
	return float(v.x & 0xffffffu) / 16777216.0;
}

// Bounced Light (R4): what the bake's rays see of the surfaces they hit is
// passed on as those surfaces are drawn (their own light x their lumel
// against their average), from last frame's copy of the atlas
struct Bounce {
	device const int* surface_patch;	// polygon x 10 + (floor, ceiling, edges) -> patch
	device const Patch* patches;
	device const float4* averages;
	texture2d<float> previous;
	float4 range;						// the redistribution's clamp and colour bleed
	int polygons;						// polygons in surface_patch
	bool on;
};

static float3 bounced(thread const Bounce& b, float4 seen, int poly, int part, float3 at)
{
	if (!b.on || seen.w > 0.5 || poly >= b.polygons)
		return seen.rgb;	// the sky is only the sky
	const float4 g = patch_light(b.range, b.patches, b.averages, b.previous, b.surface_patch[poly * 10 + part], at);
	return seen.rgb * g.rgb * g.a;
}

// Radiance seen along a ray from `o` in polygon `poly`; t: where it stops
static float3 trace_radiance(device const float4* map, device const float4* surfaces, int poly, float3 o, float3 d,
							 float far, thread float& t, thread const Bounce& bounce)
{
	const int start = poly;
	float t_prev = 0.0;
	for (int step = 0; step < 48; ++step) {
		const float4 h = map[poly * 9];
		int next = -1, edge = -1;
		const float leave = polygon_exit(map, poly, o.xy, d.xy, t_prev, next, edge);
		// The floor or ceiling, if the ray meets it inside this polygon
		float t_plane = 1e9;
		int plane = -1;
		if (d.z < -1e-5) { t_plane = (h.y - o.z) / d.z; plane = 0; }
		else if (d.z > 1e-5) { t_plane = (h.z - o.z) / d.z; plane = 1; }
		if (plane >= 0 && t_plane <= leave) {
			t = t_plane;
			// A floor or ceiling beyond reach gives the room's own light, as a
			// ray that runs out of reach sideways does (it read as black and
			// still counted: in a tall lift shaft most rays did, and the
			// lumels' few remaining hits swung between the clamps as blobs)
			if (t > far) {
				t = far;
				break;
			}
			return bounced(bounce, surfaces[poly * 10 + plane], poly, plane, o + d * t);
		}
		if (leave > far || edge < 0)
			break;
		const float z = o.z + d.z * leave;
		bool wall = next < 0;
		if (!wall) {
			const float4 hn = map[next * 9];
			wall = z < hn.y || z > hn.z;
		}
		if (wall) {
			t = leave;
			// Just inside the wall's own polygon, where its patch is
			return bounced(bounce, surfaces[poly * 10 + 2 + edge], poly, 2 + edge, o + d * (t - 2.0));
		}
		poly = next;
		t_prev = leave;
	}
	// Nothing within reach: the room's own light
	t = far;
	return 0.5 * (surfaces[start * 10].rgb + surfaces[start * 10 + 1].rgb);
}

static bool inside_polygon(device const float4* map, int poly, float2 p)
{
	const int n = int(map[poly * 9].x);
	float sign = 0.0;
	for (int i = 0; i < n; ++i) {
		const float2 a = map[poly * 9 + 1 + i].xy;
		const float2 b = map[poly * 9 + 1 + (i + 1 == n ? 0 : i + 1)].xy;
		const float c = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
		if (sign == 0.0)
			sign = c;
		else if (c * sign < 0.0)
			return false;
	}
	return true;
}

struct BakeTile {
	int4 tile;
	float4 blend;
};

struct BakeParams {
	uint rays;
	uint seed;
	uint bounce;		// Bounced Light (R4)
	uint polygons;		//   polygons in the surface -> patch lookup
	float4 range;		// the redistribution's clamp and colour bleed (u.gi_range)
};

constant float kBakeReach = 12288.0;	// 12 world units
constant float kBakeNear = 1536.0;		// hits nearer than this count for less

kernel void radiance_bake(uint2 tid [[thread_position_in_threadgroup]], uint2 group2 [[threadgroup_position_in_grid]],
						  constant BakeParams& bp [[buffer(0)]], device const BakeTile* tiles [[buffer(1)]],
						  device const Patch* patches [[buffer(2)]], device const float4* map [[buffer(3)]],
						  device const float4* surfaces [[buffer(4)]],
						  device const int* surface_patch [[buffer(5)]], device const float4* averages [[buffer(6)]],
						  texture2d<half, access::read_write> atlas [[texture(0)]],
						  texture2d<float> previous [[texture(1)]])
{
	const Bounce bounce = { surface_patch, patches, averages, previous, bp.range, int(bp.polygons), bp.bounce != 0u };
	const BakeTile job = tiles[group2.x];
	const Patch pa = patches[job.tile.x];
	const int2 l = job.tile.yz + int2(tid);
	if (l.x >= pa.rect.z || l.y >= pa.rect.w)
		return;
	const uint2 at = uint2(pa.rect.xy + l);
	const int poly = pa.info.y;
	const float4 h = map[poly * 9];
	float3 p = pa.origin.xyz + pa.du.xyz * (float(l.x) + 0.5) + pa.dv.xyz * (float(l.y) + 0.5);
	bool valid;
	if (pa.info.x == 2) {
		// A wall: within its polygon's height and not in the opening
		// to the polygon beyond (as they are now: platforms move)
		valid = p.z > h.y && p.z < h.z;
		const int beyond = int(map[poly * 9 + 1 + pa.info.z].z);
		if (valid && beyond >= 0) {
			const float4 hn = map[beyond * 9];
			valid = !(p.z > max(h.y, hn.y) && p.z < min(h.z, hn.z));
		}
	} else {
		p.z = pa.info.x == 0 ? h.y : h.z;
		valid = h.z > h.y && inside_polygon(map, poly, p.xy);
	}
	if (!valid) {
		atlas.write(half4(0.0), at);
		return;
	}
	const float3 n = pa.normal.xyz;
	const float3 t1 = normalize(abs(n.z) < 0.9 ? cross(n, float3(0, 0, 1)) : cross(n, float3(1, 0, 0)));
	const float3 t2 = cross(n, t1);
	const float3 o = p + n * 4.0;
	// A lumel with nothing baked yet starts from more rays, so it does not
	// flicker in: four times on a patch's first bake; sixteen when it is
	// uncovered on a patch already baked (a door or platform moved), where
	// it stands beside settled neighbours and is blended in only gently
	// after (32 rays left speckles and blobs round moving doors)
	const half4 old = atlas.read(at);
	const bool fresh = job.blend.x >= 1.0 || old.a < 0.5h;
	const uint rays = !fresh ? bp.rays : job.blend.x >= 1.0 ? bp.rays * 4u : bp.rays * 16u;
	float3 sum = float3(0.0);
	for (uint k = 0; k < rays; ++k) {
		// Cosine-weighted directions over the hemisphere
		const float r1 = bake_hash(uint3(at.x, at.y, bp.seed * 64u + k));
		const float r2 = bake_hash(uint3(at.y + 7919u, at.x, bp.seed * 64u + k + 31u));
		const float r = sqrt(r1), phi = 6.2831853 * r2;
		const float3 d = normalize(t1 * (r * cos(phi)) + t2 * (r * sin(phi)) + n * sqrt(max(1.0 - r1, 0.0)));
		float t;
		const float3 seen = trace_radiance(map, surfaces, poly, o, d, kBakeReach, t, bounce);
		if (seen.x < 0.0)
			continue;
		sum += seen * smoothstep(0.0, kBakeNear, t);
	}
	const float3 e = sum / float(max(rays, 1u));
	const float3 result = fresh ? e : mix(float3(old.rgb), e, job.blend.x);
	atlas.write(half4(half3(result), 1.0h), at);
}

// Each baked patch's average over its lumels, and how many there are
// The average over a baked patch's whole group (adjacent coplanar surfaces
// with the same texture and light, so a light gradient runs across their
// shared edges instead of resetting at each polygon), written to every
// member's slot. Two baked patches of one group write the same value.
kernel void radiance_average(uint group [[threadgroup_position_in_grid]], uint ti [[thread_index_in_threadgroup]],
							 device const int* baked [[buffer(0)]], device const Patch* patches [[buffer(1)]],
							 device float4* averages [[buffer(2)]], device const int* group_start [[buffer(3)]],
							 device const int* group_members [[buffer(4)]], texture2d<half, access::read> atlas [[texture(0)]])
{
	threadgroup float4 partial[64];
	const int index = baked[group];
	const int g = patches[index].info.w;
	const int first = group_start[g], last = group_start[g + 1];
	float4 sum = float4(0.0);
	for (int m = first; m < last; ++m) {
		const Patch pa = patches[group_members[m]];
		const int count = pa.rect.z * pa.rect.w;
		for (int i = int(ti); i < count; i += 64) {
			const half4 c = atlas.read(uint2(pa.rect.xy + int2(i % pa.rect.z, i / pa.rect.z)));
			sum += float4(float3(c.rgb) * float(c.a), float(c.a));
		}
	}
	partial[ti] = sum;
	threadgroup_barrier(mem_flags::mem_threadgroup);
	for (uint stride = 32; stride > 0; stride >>= 1) {
		if (ti < stride)
			partial[ti] += partial[ti + stride];
		threadgroup_barrier(mem_flags::mem_threadgroup);
	}
	if (ti == 0) {
		const float4 average = float4(partial[0].rgb / max(partial[0].a, 1e-6), partial[0].a);
		for (int m = first; m < last; ++m)
			averages[group_members[m]] = average;
	}
}

// Final pass: world colour -> presentation target, with the gamma
// adjustment the OpenGL path applies in its S_Gamma blit.
struct BlitOut {
	float4 position [[position]];
	float2 uv;
};

vertex BlitOut blit_vertex(uint vid [[vertex_id]])
{
	const float2 corners[4] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1) };
	BlitOut out;
	out.position = float4(corners[vid], 0.0, 1.0);
	out.uv = float2(corners[vid].x * 0.5 + 0.5, 0.5 - corners[vid].y * 0.5);
	return out;
}

// Ambient shadows (Round 12): screen-space ambient occlusion from the
// world pass's distance image. Each pixel's world position comes back
// from its distance along the pixel's ray; its normal from the position's
// screen derivatives; a hemisphere of sample points around it, rotated
// per pixel by interleaved gradient noise, is projected back to the
// screen and counted as occluded where the scene is nearer than the
// point (with a range check, so distant geometry does not shadow). The
// world blit (DurandalGL) blurs the result with a depth-aware 3x3 and
// multiplies the world image by it, fading with distance so haze and far
// halls are left alone.
struct AOParams {
	float4x4 view_projection;	// world -> clip
	float4x4 inverse;			// clip -> world
	float4 camera;
	float4 screen;				// 1/width, 1/height, width, height
	float radius;				// world units
	float strength;
	float far;					// no occlusion beyond this distance
	float view;					// 1: show the occlusion instead of the world (development)
	float4 grid;				// traced (R3): the polygon grid's origin x, y, cells per world unit
	int4 traced;				//   x on, y columns, z rows, w polygons in the occluder lists
	int4 viewer;				//   x the viewer's polygon
};

// Traced ambient shadows (R3, Rampant): the polygon a point is in, from the
// level's grid (DurandalLights::BuildGrid), its heights deciding between
// rooms stacked over each other; -1 if none
static int grid_polygon(constant AOParams& p, device const int2* cells, device const int* indices,
						device const float4* map, float3 at);

// The figure whose card, as the viewer sees it, a point lies on (a sprite's
// own pixels must not be shadowed by its own card); far away if none
// A figure is drawn as an upright card through its position, square to
// the view's yaw (`forward`, RenderRasterize_Metal's sprite transform), so
// a pixel is the figure's own only when it lies on that card and faces the
// way the card does. A floor, a ceiling or a wall beside it never is: the
// old test took everything within 48 units of a card facing the
// camera-to-figure ray, so a corpse's strip of floor and of the walls
// either side skipped the corpse while the rest did not (a lighter bar in
// the sprite's plane, plainest in dark corners; the owner, 2 Oct 2026), and
// a figure off to the side of the view was not taken as itself and shaded
// itself
static float2 figure_under(thread const Occluders& occ, int poly, float3 at, float3 n, float2 forward)
{
	if (poly < 0 || poly >= occ.polygon_count || abs(n.z) > 0.7 || abs(dot(n.xy, forward)) < 0.7)
		return float2(1e9);
	const float2 side = float2(-forward.y, forward.x);
	const int2 range = occ.polygons[poly];
	for (int k = 0; k < range.y; ++k) {
		const Occluder o = occ.list[occ.indices[range.x + k]];
		const float2 r = at.xy - o.position.xy;
		const float h = dot(r, side), z = at.z - o.position.z;
		if (abs(dot(r, forward)) < 16.0 && h >= min(o.extent.x, -o.extent.y) - 8.0 && h <= max(o.extent.y, -o.extent.x) + 8.0 &&
			z >= o.extent.z - 8.0 && z <= o.extent.w + 8.0)
			return o.position.xy;
	}
	return float2(1e9);
}

static float3 ao_world(constant AOParams& p, float2 uv, float distance)
{
	const float4 clip = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 1.0, 1.0);
	float4 w = p.inverse * clip;
	w /= w.w;
	const float3 dir = normalize(w.xyz - p.camera.xyz);
	return p.camera.xyz + dir * distance;
}

constant float3 kAOKernel[12] = {
	float3( 0.53,  0.20,  0.82), float3(-0.41,  0.66,  0.63), float3( 0.10, -0.76,  0.64),
	float3(-0.70, -0.30,  0.65), float3( 0.80, -0.45,  0.40), float3(-0.20,  0.30,  0.93),
	float3( 0.30,  0.85,  0.43), float3(-0.85,  0.20,  0.49), float3( 0.55, -0.15,  0.82),
	float3(-0.10, -0.40,  0.91), float3( 0.35,  0.55,  0.76), float3(-0.50, -0.70,  0.51)
};

static int grid_polygon(constant AOParams& p, device const int2* cells, device const int* indices,
						device const float4* map, float3 at)
{
	const int2 c = int2(floor((at.xy - p.grid.xy) * p.grid.z));
	if (any(c < 0) || c.x >= p.traced.y || c.y >= p.traced.z)
		return -1;
	const int2 range = cells[c.y * p.traced.y + c.x];
	int found = -1;
	bool ambiguous = false;
	for (int k = 0; k < range.y; ++k) {
		const int poly = indices[range.x + k];
		const float4 h = map[poly * 9];
		if (at.z < h.y - 16.0 || at.z > h.z + 16.0)
			continue;
		if (inside_polygon(map, poly, at.xy)) {
			if (found >= 0) {
				ambiguous = true;
				break;
			}
			found = poly;
		}
	}
	// Two polygons in the same space (5D): the one the viewer sees the
	// point in is where the view ray from the viewer's polygon arrives
	if (ambiguous && p.viewer.x >= 0) {
		int poly = p.viewer.x;
		const float2 a = p.camera.xy, d = at.xy - p.camera.xy;
		float t_prev = 0.0;
		for (int step = 0; step < 64 && poly >= 0; ++step) {
			int next = -1, edge = -1;
			const float leave = polygon_exit(map, poly, a, d, t_prev, next, edge);
			if (leave >= 1.0 || next < 0)
				return poly;
			poly = next;
			t_prev = leave;
		}
	}
	return found;
}

// Development: DURANDAL_TRACE_VIEW=1 draws the world by tracing instead:
// one ray per pixel walked through the map from the viewer's polygon,
// what it meets shaded from the surface table (DurandalSurfaces.h) as a
// reflection is. Set beside the drawn frame it shows whether the table
// places every texture as the rasteriser does
struct TraceViewParams {
	float4x4 inverse;		// clip -> world
	int4 viewer;			// x: the viewer's polygon
};

fragment float4 trace_view_fragment(BlitOut in [[stage_in]], constant Uniforms& u [[buffer(0)]],
									constant TraceViewParams& p [[buffer(1)]], device const float4* map [[buffer(2)]],
									device const float4* surfaces [[buffer(3)]], texture2d_array<float> walls [[texture(0)]],
									texture2d<float> sky [[texture(1)]])
{
	const float4 clip = float4(in.uv.x * 2.0 - 1.0, 1.0 - in.uv.y * 2.0, 1.0, 1.0);
	float4 w = p.inverse * clip;
	w /= w.w;
	const float3 o = u.camera.xyz;
	const float3 d = normalize(w.xyz - o);
	SurfaceHit hit;
	if (p.viewer.x < 0 || !trace_surfaces(map, p.viewer.x, o, d, 65536.0, hit))
		return float4(0.0, 0.0, 0.0, 1.0);
	return float4(shade_hit(u, surfaces, map, walls, sky, hit, d, hit.t), 1.0);
}

fragment float ao_fragment(BlitOut in [[stage_in]], constant AOParams& p [[buffer(0)]],
						   texture2d<float> dist [[texture(0)]],
						   device const float4* map [[buffer(1)]], device const int2* grid_cells [[buffer(2)]],
						   device const int* grid_indices [[buffer(3)]], device const Occluder* occluders [[buffer(4)]],
						   device const int2* occluder_polygons [[buffer(5)]], device const int* occluder_indices [[buffer(6)]],
						   texture2d_array<float> masks [[texture(1)]])
{
	constexpr sampler near(filter::nearest, address::clamp_to_edge);
	const float d = dist.sample(near, in.uv).r;
	if (d >= p.far)	// beyond the reach, the sky, liquid surfaces and the weapon in hand (all far)
		return 1.0;
	const float3 P = ao_world(p, in.uv, d);
	// The normal from the neighbouring distances: on each axis the
	// neighbour nearer in depth and never a far one (the sky, a liquid,
	// the weapon), so silhouettes, the water line and the gun's edge do
	// not bend it (screen derivatives did, and drew dotted outlines)
	auto pick = [&](float2 a, float2 b, thread float2& uv, thread float& dn) -> bool {
		const float da = dist.sample(near, a).r, db = dist.sample(near, b).r;
		const bool va = da < p.far, vb = db < p.far;
		if (!va && !vb)
			return false;
		if (va && (!vb || abs(da - d) <= abs(db - d))) { uv = a; dn = da; }
		else { uv = b; dn = db; }
		return true;
	};
	float2 ux, uy;
	float dx, dy;
	if (!pick(in.uv - float2(p.screen.x, 0.0), in.uv + float2(p.screen.x, 0.0), ux, dx) ||
		!pick(in.uv - float2(0.0, p.screen.y), in.uv + float2(0.0, p.screen.y), uy, dy))
		return 1.0;
	float3 Tx = ao_world(p, ux, dx) - P;
	if (ux.x < in.uv.x)
		Tx = -Tx;
	float3 Ty = ao_world(p, uy, dy) - P;
	if (uy.y < in.uv.y)
		Ty = -Ty;
	float3 N = cross(Tx, Ty);
	if (length_squared(N) < 1e-6)
		return 1.0;
	N = normalize(N);
	if (dot(N, p.camera.xyz - P) < 0.0)
		N = -N;
	// A tangent frame around the normal, turned per pixel
	const float2 pixel = floor(in.position.xy);
	const float angle = 6.2831853 * fract(52.9829189 * fract(0.06711056 * pixel.x + 0.00583715 * pixel.y));
	const float3 helper = abs(N.z) < 0.9 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	const float3 T0 = normalize(cross(helper, N));
	const float3 B0 = cross(N, T0);
	const float ca = cos(angle), sa = sin(angle);
	const float3 T = T0 * ca + B0 * sa;
	const float3 B = cross(N, T);
	// Traced (R3): short rays walked through the map from the point, and the
	// figures standing near it, so what is off screen still shades it
	// Traced only near the viewer (within 10 WU, where it shows; the
	// screen-space estimate beyond fades out anyway): eight rays to 4 WU,
	// four beyond (every film's worst frames were in this pass)
	if (p.traced.x != 0 && d < 10240.0) {
		const int ray_count = d < 4096.0 ? 8 : 4;
		const float3 o = P + N * 8.0;
		const int poly = grid_polygon(p, grid_cells, grid_indices, map, o);
		// Development: DURANDAL_AO_VIEW=6 compares the stored distance with the
		// view ray walked through the map from the viewer's polygon (mid grey:
		// the same; lighter: stored further than the map says)
		if (p.view == 6.0) {
			SurfaceHit hit;
			const float3 dir = normalize(P - p.camera.xyz);
			if (p.viewer.x < 0 || !trace_surfaces(map, p.viewer.x, p.camera.xyz, dir, 65536.0, hit))
				return 0.0;
			return saturate(0.5 + (d - hit.t) / 256.0);
		}
		// Development: DURANDAL_AO_VIEW=5 shows why the grid finds nothing: 0.2
		// outside it, 0.4 an empty cell, 0.6 candidates beside the point, 0.8
		// candidates above or below it, 1 found
		if (p.view == 5.0) {
			const int2 c = int2(floor((o.xy - p.grid.xy) * p.grid.z));
			if (any(c < 0) || c.x >= p.traced.y || c.y >= p.traced.z)
				return 0.2;
			const int2 range = grid_cells[c.y * p.traced.y + c.x];
			if (range.y == 0)
				return 0.4;
			bool beside = false;
			for (int k = 0; k < range.y; ++k) {
				const int q = grid_indices[range.x + k];
				if (inside_polygon(map, q, o.xy))
					beside = true;
			}
			return poly >= 0 ? 1.0 : beside ? 0.8 : 0.6;
		}
		// Development: DURANDAL_AO_VIEW=4 shows the polygon each pixel is placed in
		if (p.view == 4.0)
			return poly < 0 ? 1.0 : 0.1 + 0.8 * fract(float(poly) * 0.618034);
		if (poly >= 0) {
			Occluders occ = { occluders, occluder_polygons, occluder_indices, masks, p.traced.w, float2(1e9), nullptr, masks };
			// The view's yaw: the horizontal heading of the screen's centre
			float2 forward = ao_world(p, float2(0.5), 1024.0).xy - p.camera.xy;
			forward = length_squared(forward) > 1e-4 ? normalize(forward) : float2(1.0, 0.0);
			occ.self = figure_under(occ, poly, P, N, forward);
			// Development: DURANDAL_AO_VIEW=7 blacks out the pixels taken as a
			// figure's own (they skip that figure's card)
			if (p.view == 7.0)
				return any(occ.self < float2(1e8)) ? 0.0 : 1.0;
			// The few figures within reach, picked once for all eight rays
			int near_figures[4];
			int near_count = 0;
			if (poly < occ.polygon_count) {
				const int2 range = occ.polygons[poly];
				for (int k = 0; k < range.y && near_count < 4; ++k) {
					const int index = occ.indices[range.x + k];
					const Occluder f = occ.list[index];
					const float reach = p.radius + max(abs(f.extent.x), abs(f.extent.y));
					if (length_squared(f.position.xy - o.xy) < reach * reach && any(abs(f.position.xy - occ.self) >= 0.5))
						near_figures[near_count++] = index;
				}
			}
			// Eight directions about the normal, cosine-weighted rings (Rampant
			// spends its headroom on the smoother result)
			const float4 dirs[8] = { float4(0.50, 0.00, 0.866, 0.0), float4(0.0, 0.87, 0.50, 0.0),
									 float4(-0.71, 0.0, 0.71, 0.0), float4(0.0, -0.94, 0.34, 0.0),
									 float4(0.61, 0.61, 0.50, 0.0), float4(-0.40, 0.40, 0.82, 0.0),
									 float4(-0.66, -0.66, 0.36, 0.0), float4(0.30, -0.30, 0.90, 0.0) };
			float traced = 0.0;
			for (int i = 0; i < ray_count; ++i) {
				const float3 dir = normalize(T * dirs[i].x + B * dirs[i].y + N * dirs[i].z);
				SurfaceHit hit;
				float hit_weight = 0.0;
				if (trace_surfaces(map, poly, o, dir, p.radius, hit)) {
					const float f = 1.0 - saturate(hit.t / p.radius);
					hit_weight = f * f;
				}
				float through = 1.0;
				for (int k = 0; k < near_count; ++k)
					through *= figure_through(occ, occ.list[near_figures[k]], o, dir * p.radius, 0.0, 1.0, p.radius * 0.5);
				// Development: DURANDAL_AO_VIEW=2 the surfaces only, 3 the figures only
				traced += p.view == 2.0 ? hit_weight : p.view == 3.0 ? 1.0 - through : max(hit_weight, 1.0 - through);
			}
			return saturate(1.0 - p.strength * traced / float(ray_count));
		}
	}
	float occlusion = 0.0;
	for (int i = 0; i < 8; ++i) {
		const float scale = mix(0.15, 1.0, float(i * i) / 49.0);
		const float3 k = kAOKernel[i];
		const float3 Q = P + (T * k.x + B * k.y + N * k.z) * (p.radius * scale) + N * (p.radius * 0.03);
		const float4 c = p.view_projection * float4(Q, 1.0);
		if (c.w <= 1.0)
			continue;
		const float2 suv = float2(c.x / c.w * 0.5 + 0.5, 0.5 - c.y / c.w * 0.5);
		if (any(suv < 0.0) || any(suv > 1.0))
			continue;
		const float ds = dist.sample(near, suv).r;
		const float dq = length(Q - p.camera.xyz);
		if (ds < dq - p.radius * 0.02) {
			const float range = smoothstep(0.0, 1.0, p.radius / max(abs(d - ds), 1e-3));
			occlusion += range;
		}
	}
	return saturate(1.0 - p.strength * occlusion / 8.0);
}

// Bloom (E1): fed only by the glow image. A half-resolution chain of
// downsamples (13 taps, Jimenez 2014) and tent-filtered upsamples added back
// up the chain; the first downsample converts the glow to linear light, so
// the bloom is linear and adds like light in the output pass.
struct BloomParams {
	float2 texel;	// 1 / source size
	int first;		// source is the glow image (display units)
	float radius;
};

static float3 glow_to_linear(float3 c)
{
	c = saturate(c);
	return select(pow((c + 0.055) / 1.055, float3(2.4)), c / 12.92, c <= 0.04045);
}

fragment float4 bloom_down_fragment(BlitOut in [[stage_in]], constant BloomParams& p [[buffer(0)]],
									texture2d<float> src [[texture(0)]])
{
	constexpr sampler s(filter::linear, address::clamp_to_edge);
	auto at = [&](float x, float y) {
		const float3 c = src.sample(s, in.uv + float2(x, y) * p.texel).rgb;
		return p.first ? glow_to_linear(c) : c;
	};
	const float3 e = at(0, 0);
	const float3 corners = at(-2, -2) + at(2, -2) + at(-2, 2) + at(2, 2);
	const float3 edges = at(0, -2) + at(-2, 0) + at(2, 0) + at(0, 2);
	const float3 inner = at(-1, -1) + at(1, -1) + at(-1, 1) + at(1, 1);
	return float4(e * 0.125 + corners * 0.03125 + edges * 0.0625 + inner * 0.125, 1.0);
}

fragment float4 bloom_up_fragment(BlitOut in [[stage_in]], constant BloomParams& p [[buffer(0)]],
								  texture2d<float> src [[texture(0)]])
{
	constexpr sampler s(filter::linear, address::clamp_to_edge);
	const float2 d = p.texel * p.radius;
	auto at = [&](float x, float y) { return src.sample(s, in.uv + float2(x, y) * d).rgb; };
	const float3 c = (at(-1, -1) + at(1, -1) + at(-1, 1) + at(1, 1)) +
		(at(0, -1) + at(-1, 0) + at(1, 0) + at(0, 1)) * 2.0 + at(0, 0) * 4.0;
	return float4(c / 16.0, 1.0);
}

// Anti-aliased edges: the previous world frame as the backdrop, so the
// void still smears (DurandalMetal.mm)
fragment float4 backdrop_fragment(BlitOut in [[stage_in]], texture2d<float> previous [[texture(0)]])
{
	return previous.read(uint2(in.position.xy));
}

fragment float4 gamma_fragment(BlitOut in [[stage_in]], constant float& gamma [[buffer(0)]],
							   texture2d<float> tex [[texture(0)]])
{
	constexpr sampler s(filter::nearest, address::clamp_to_edge);
	const float4 c = tex.sample(s, in.uv);
	return float4(pow(c.rgb, float3(gamma)), 1.0);
}
)MSL";

#endif
