/*
	DurandalRadiance.cpp — Durandal project

	See DurandalRadiance.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "cseries.h"
#include "DurandalRadiance.h"
#include "DurandalMetal.h"
#include "DurandalShading.h"
#include "DurandalGlow.h"

#include "map.h"
#include "render.h"
#include "RenderSortPoly.h"
#include "interface.h"
#include "collection_definition.h"
#include "textures.h"
#include "shape_descriptors.h"
#include "lightsource.h"
#include "platforms.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>

namespace DurandalRadiance {

namespace {

const float kLumel = 128.0f;		// world units per lumel: 8 per world unit
const int kLargest = 256;			// lumels across a patch at most
const int kAtlasWidth = 2048;
const int kRays = 8;				// per lumel per bake
const int kTarget = 192;			// samples per lumel before a patch is left alone
const int kRefreshBakes = 48;		// bakes a patch near a moving platform gets after it last moved,
const float kRefreshBlend = 1.0f / 24;	// each blended in this gently (the samples were clamped to
									// 64 before, an 11% blend of 8 noisy rays every frame a door
									// moved: the ripple round door frames seen in QA)
const int kTileBudget = 384;		// 8x8 tiles baked per frame
const int kBounceTiles = 128;		// Bounced Light: of those, settled tiles refreshed per frame (round robin)
const float kBounceBlend = 1.0f / 16;	// each refresh blended in this gently
const float kLampBoost = 2.0f;		// reviewed wall lights give off this much more
const float kSky = 0.9f;			// sky light through landscape surfaces

struct State {
	uint64_t key = 0;
	std::vector<DurandalMetal::Patch> patches;
	std::vector<int> samples;
	std::vector<int> refresh;		// bakes still owed at kRefreshBlend (moving platforms)
	std::vector<int> floor_patch, ceiling_patch;	// per polygon
	std::vector<int> wall_patch;					// per polygon x 8 edges
	std::vector<int> side_patch;					// per side
	std::vector<simd_float2> heights;				// per polygon, last frame
	uint32_t frame = 0;
	size_t bounce_cursor = 0;						// Bounced Light: where the refresh round got to
	std::vector<int> bounce_tile;					// per patch: its next tile to refresh (large patches go a slice a frame)
	bool ready = false;
};
State state;

uint64_t level_key()
{
	uint64_t h = 1469598103934665603ull;
	auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
	mix(dynamic_world->polygon_count);
	mix(dynamic_world->side_count);
	mix(dynamic_world->endpoint_count);
	for (int i = 0; i < dynamic_world->endpoint_count; ++i)
	{
		const endpoint_data* e = get_endpoint_data(i);
		mix(uint16_t(e->vertex.x) | (uint32_t(uint16_t(e->vertex.y)) << 16));
	}
	return h;
}

// The heights a polygon's surfaces can span (platforms move within theirs)
void polygon_range(const polygon_data* polygon, float& low, float& high)
{
	low = polygon->floor_height;
	high = polygon->ceiling_height;
	if (polygon->type == _polygon_is_platform)
		if (platform_data* platform = get_platform_data(polygon->permutation))
		{
			low = std::min<float>(low, platform->minimum_floor_height);
			high = std::max<float>(high, platform->maximum_ceiling_height);
		}
}

int lumels(float extent, float& step)
{
	int n = std::max(1, int(std::lround(extent / kLumel)));
	n = std::min(n, kLargest);
	step = extent / n;
	return n;
}

void build_layout()
{
	State s;
	s.key = level_key();
	const int polygons = dynamic_world->polygon_count;
	s.floor_patch.assign(polygons, -1);
	s.ceiling_patch.assign(polygons, -1);
	s.wall_patch.assign(size_t(polygons) * MAXIMUM_VERTICES_PER_POLYGON, -1);
	s.side_patch.assign(dynamic_world->side_count, -1);
	s.heights.assign(polygons, simd_make_float2(0, 0));

	for (int p = 0; p < polygons; ++p)
	{
		const polygon_data* polygon = get_polygon_data(p);
		const int n = std::min<int>(polygon->vertex_count, MAXIMUM_VERTICES_PER_POLYGON);
		if (n < 3)
			continue;
		s.heights[p] = simd_make_float2(polygon->floor_height, polygon->ceiling_height);
		float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f, cx = 0, cy = 0;
		for (int i = 0; i < n; ++i)
		{
			const world_point2d v = get_endpoint_data(polygon->endpoint_indexes[i])->vertex;
			minx = std::min<float>(minx, v.x); maxx = std::max<float>(maxx, v.x);
			miny = std::min<float>(miny, v.y); maxy = std::max<float>(maxy, v.y);
			cx += v.x; cy += v.y;
		}
		cx /= n; cy /= n;

		// Floor and ceiling
		for (int ceiling = 0; ceiling < 2; ++ceiling)
		{
			DurandalMetal::Patch pa = {};
			float sx, sy;
			const int w = lumels(std::max(maxx - minx, 1.0f), sx), h = lumels(std::max(maxy - miny, 1.0f), sy);
			pa.origin = simd_make_float4(minx, miny, 0, 0);
			pa.u = simd_make_float4(1 / sx, 0, 0, 0);
			pa.v = simd_make_float4(0, 1 / sy, 0, 0);
			pa.du = simd_make_float4(sx, 0, 0, 0);
			pa.dv = simd_make_float4(0, sy, 0, 0);
			pa.normal = simd_make_float4(0, 0, ceiling ? -1 : 1, 0);
			pa.rect = simd_make_int4(0, 0, w, h);
			pa.info = simd_make_int4(ceiling ? 1 : 0, p, 0, 0);
			(ceiling ? s.ceiling_patch : s.floor_patch)[p] = int(s.patches.size());
			s.patches.push_back(pa);
		}

		// Walls, where the polygon has a side
		float low, high;
		polygon_range(polygon, low, high);
		for (int i = 0; i < n; ++i)
		{
			const short side = polygon->side_indexes[i];
			if (side == NONE || side >= dynamic_world->side_count || high <= low)
				continue;
			const world_point2d a = get_endpoint_data(polygon->endpoint_indexes[i])->vertex;
			const world_point2d b = get_endpoint_data(polygon->endpoint_indexes[(i + 1) % n])->vertex;
			const float dx = b.x - a.x, dy = b.y - a.y, length = std::sqrt(dx * dx + dy * dy);
			if (length < 1)
				continue;
			const float ux = dx / length, uy = dy / length;
			float nx = -uy, ny = ux;
			if (nx * (cx - a.x) + ny * (cy - a.y) < 0)
			{
				nx = -nx;
				ny = -ny;
			}
			DurandalMetal::Patch pa = {};
			float sw, sh;
			const int w = lumels(length, sw), h = lumels(high - low, sh);
			pa.origin = simd_make_float4(a.x, a.y, low, 0);
			pa.u = simd_make_float4(ux / sw, uy / sw, 0, 0);
			pa.v = simd_make_float4(0, 0, 1 / sh, 0);
			pa.du = simd_make_float4(ux * sw, uy * sw, 0, 0);
			pa.dv = simd_make_float4(0, 0, sh, 0);
			pa.normal = simd_make_float4(nx, ny, 0, 0);
			pa.rect = simd_make_int4(0, 0, w, h);
			pa.info = simd_make_int4(2, p, i, 0);
			s.wall_patch[size_t(p) * MAXIMUM_VERTICES_PER_POLYGON + i] = int(s.patches.size());
			s.side_patch[side] = int(s.patches.size());
			s.patches.push_back(pa);
		}
	}

	// Shelf packing, tallest first
	std::vector<int> order(s.patches.size());
	for (size_t i = 0; i < order.size(); ++i)
		order[i] = int(i);
	std::sort(order.begin(), order.end(), [&](int a, int b) { return s.patches[a].rect.w > s.patches[b].rect.w; });
	int x = 0, y = 0, row = 0;
	for (int i : order)
	{
		DurandalMetal::Patch& pa = s.patches[i];
		if (x + pa.rect.z > kAtlasWidth)
		{
			x = 0;
			y += row;
			row = 0;
		}
		pa.rect.x = x;
		pa.rect.y = y;
		x += pa.rect.z;
		row = std::max(row, int(pa.rect.w));
	}
	const int height = ((y + row + 63) / 64) * 64;

	// Groups: adjacent coplanar surfaces with the same texture and light
	// share one average, so a light gradient runs across their shared
	// edges instead of resetting at each polygon (a rectangle on a
	// ceiling made of several polygons). Platforms keep to themselves.
	std::vector<int> parent(s.patches.size());
	for (size_t i = 0; i < parent.size(); ++i)
		parent[i] = int(i);
	std::function<int(int)> find = [&](int a) { return parent[a] == a ? a : (parent[a] = find(parent[a])); };
	auto unite = [&](int a, int b) {
		if (a < 0 || b < 0) return;
		a = find(a); b = find(b);
		if (a != b) parent[std::max(a, b)] = std::min(a, b);
	};
	for (int p = 0; p < polygons; ++p)
	{
		const polygon_data* polygon = get_polygon_data(p);
		const int n = std::min<int>(polygon->vertex_count, MAXIMUM_VERTICES_PER_POLYGON);
		if (polygon->type == _polygon_is_platform)
			continue;
		for (int i = 0; i < n; ++i)
		{
			const short q = polygon->adjacent_polygon_indexes[i];
			if (q == NONE || q < 0 || q >= polygons)
				continue;
			const polygon_data* other = get_polygon_data(q);
			if (other->type == _polygon_is_platform)
				continue;
			if (polygon->floor_height == other->floor_height && polygon->floor_texture == other->floor_texture &&
				polygon->floor_lightsource_index == other->floor_lightsource_index &&
				polygon->floor_transfer_mode == other->floor_transfer_mode)
				unite(s.floor_patch[p], s.floor_patch[q]);
			if (polygon->ceiling_height == other->ceiling_height && polygon->ceiling_texture == other->ceiling_texture &&
				polygon->ceiling_lightsource_index == other->ceiling_lightsource_index &&
				polygon->ceiling_transfer_mode == other->ceiling_transfer_mode)
				unite(s.ceiling_patch[p], s.ceiling_patch[q]);
		}
	}
	// Walls: segments that continue each other along one line, meeting at
	// an endpoint, with the same texture, light and heights
	struct WallEnd { int patch; float dx, dy; shape_descriptor texture; short light; float low, high; };
	std::vector<std::vector<WallEnd>> at_endpoint(std::max<int>(dynamic_world->endpoint_count, 1));
	for (int p = 0; p < polygons; ++p)
	{
		const polygon_data* polygon = get_polygon_data(p);
		const int n = std::min<int>(polygon->vertex_count, MAXIMUM_VERTICES_PER_POLYGON);
		if (polygon->type == _polygon_is_platform)
			continue;
		for (int i = 0; i < n; ++i)
		{
			const int patch = s.wall_patch[size_t(p) * MAXIMUM_VERTICES_PER_POLYGON + i];
			const short side = polygon->side_indexes[i];
			if (patch < 0 || side == NONE || side >= dynamic_world->side_count)
				continue;
			const side_data* sd = get_side_data(side);
			const short e0 = polygon->endpoint_indexes[i], e1 = polygon->endpoint_indexes[(i + 1) % n];
			const world_point2d a = get_endpoint_data(e0)->vertex, b = get_endpoint_data(e1)->vertex;
			const float dx = b.x - a.x, dy = b.y - a.y, length = std::sqrt(dx * dx + dy * dy);
			if (length < 1)
				continue;
			const DurandalMetal::Patch& pa = s.patches[patch];
			const WallEnd from_a = { patch, dx / length, dy / length, sd->primary_texture.texture, sd->primary_lightsource_index,
									 pa.origin.z, pa.origin.z + pa.dv.z * pa.rect.w };
			const WallEnd from_b = { patch, -dx / length, -dy / length, from_a.texture, from_a.light, from_a.low, from_a.high };
			if (e0 >= 0 && e0 < int(at_endpoint.size())) at_endpoint[e0].push_back(from_a);
			if (e1 >= 0 && e1 < int(at_endpoint.size())) at_endpoint[e1].push_back(from_b);
		}
	}
	for (const auto& ends : at_endpoint)
		for (size_t i = 0; i < ends.size(); ++i)
			for (size_t j = i + 1; j < ends.size(); ++j)
			{
				const WallEnd& u = ends[i];
				const WallEnd& v = ends[j];
				// Continuing each other: pointing away from the endpoint in opposite directions
				if (u.dx * v.dx + u.dy * v.dy > -0.98f)
					continue;
				if (u.texture != v.texture || u.light != v.light || u.low != v.low || u.high != v.high)
					continue;
				unite(u.patch, v.patch);
			}
	std::vector<int> group_of(s.patches.size(), -1);
	int groups = 0;
	for (size_t i = 0; i < s.patches.size(); ++i)
	{
		const int root = find(int(i));
		if (group_of[root] < 0)
			group_of[root] = groups++;
		group_of[i] = group_of[root];
		s.patches[i].info.w = group_of[i];
	}
	std::vector<int> group_start(groups + 1, 0), group_members(s.patches.size());
	for (int g : group_of)
		++group_start[g + 1];
	for (int g = 0; g < groups; ++g)
		group_start[g + 1] += group_start[g];
	{
		std::vector<int> fill(group_start.begin(), group_start.end() - 1);
		for (size_t i = 0; i < s.patches.size(); ++i)
			group_members[fill[group_of[i]]++] = int(i);
	}

	s.samples.assign(s.patches.size(), 0);
	s.refresh.assign(s.patches.size(), 0);
	s.bounce_tile.assign(s.patches.size(), 0);
	s.ready = height <= 16384;
	if (s.ready)
		DurandalMetal::SetRadianceLayout(s.patches, kAtlasWidth, height, group_start, group_members);
	state = std::move(s);
}

// A texture's average colour at full brightness, and whether it is one of
// the reviewed wall lights
struct Colour {
	simd_float3 rgb;
	bool lamp;
	bool known;		// false: no bitmap or colour table to average (grey)
};

Colour texture_colour(shape_descriptor texture)
{
	static uint32_t generation = 0;
	static std::unordered_map<uint16_t, Colour> cache;
	if (generation != DurandalShading::Generation())
	{
		cache.clear();
		generation = DurandalShading::Generation();
	}
	auto it = cache.find(texture);
	if (it != cache.end())
		return it->second;

	Colour colour = { simd_make_float3(0.5f, 0.5f, 0.5f), false, false };
	const short code = GET_DESCRIPTOR_COLLECTION(texture), shape = GET_DESCRIPTOR_SHAPE(texture);
	bitmap_definition* bitmap = nullptr;
	void* tables = nullptr;
	extended_get_shape_bitmap_and_shading_table(code, shape, &bitmap, &tables, _shading_normal);
	const DurandalShading::Ramps* ramps = DurandalShading::Get(GET_COLLECTION(code), GET_COLLECTION_CLUT(code));
	if (bitmap && ramps && bitmap->bytes_per_row != NONE)
	{
		double sum[3] = { 0, 0, 0 };
		long count = 0;
		const bool columns = (bitmap->flags & _COLUMN_ORDER_BIT) != 0;
		const int lines = columns ? bitmap->width : bitmap->height;
		const int length = columns ? bitmap->height : bitmap->width;
		for (int l = 0; l < lines; ++l)
		{
			const uint8* p = bitmap->row_addresses[l];
			for (int i = 0; i < length; ++i)
			{
				const uint8_t* c = ramps->data[1][p[i]];
				sum[0] += c[0]; sum[1] += c[1]; sum[2] += c[2];
				++count;
			}
		}
		if (count)
		{
			colour.rgb = simd_make_float3(sum[0] / count, sum[1] / count, sum[2] / count) / 255.0f;
			colour.known = true;
		}
		const short bitmap_index = get_bitmap_index(GET_COLLECTION(code), shape);
		colour.lamp = bitmap_index != NONE &&
			DurandalGlow::ModeFor(GET_COLLECTION(code), bitmap_index, DurandalGlow::Fingerprint(bitmap)) != DurandalGlow::kNone;
	}
	cache[texture] = colour;
	return colour;
}

float light(short index, int32 delta = 0)
{
	if (index == NONE)
		return 0.5f;
	return PIN(get_light_intensity(index) + delta, 0, FIXED_ONE) / float(FIXED_ONE);
}

// Bounced Light: the sky gives off the level's own sky colour
bool sky_from_landscape = false;

// What a surface gives off: its authored brightness x its colour
simd_float4 surface(shape_descriptor texture, short transfer_mode, float brightness)
{
	static const simd_float3 sky = { 0.55f, 0.62f, 0.72f };
	if (transfer_mode == _xfer_landscape)
	{
		if (sky_from_landscape && texture != UNONE)
		{
			const Colour c = texture_colour(texture);
			if (c.known)
				return simd_make_float4(c.rgb * kSky, 1);
		}
		return simd_make_float4(sky * kSky, 1);
	}
	if (texture == UNONE)
		return simd_make_float4(0.25f * brightness, 0.25f * brightness, 0.25f * brightness, 0);
	const Colour c = texture_colour(texture);
	const simd_float3 rgb = c.rgb * brightness * (c.lamp ? kLampBoost : 1.0f);
	return simd_make_float4(rgb, 0);
}

void build_surfaces(std::vector<simd_float4>& out)
{
	const int polygons = dynamic_world->polygon_count;
	out.assign(size_t(std::max(polygons, 1)) * 10, simd_make_float4(0, 0, 0, 0));
	for (int p = 0; p < polygons; ++p)
	{
		const polygon_data* polygon = get_polygon_data(p);
		simd_float4* o = &out[size_t(p) * 10];
		o[0] = surface(polygon->floor_texture, polygon->floor_transfer_mode, light(polygon->floor_lightsource_index));
		o[1] = surface(polygon->ceiling_texture, polygon->ceiling_transfer_mode, light(polygon->ceiling_lightsource_index));
		const int n = std::min<int>(polygon->vertex_count, MAXIMUM_VERTICES_PER_POLYGON);
		for (int i = 0; i < n; ++i)
		{
			const short index = polygon->side_indexes[i];
			if (index == NONE || index >= dynamic_world->side_count)
			{
				o[2 + i] = (o[0] + o[1]) * 0.5f;
				continue;
			}
			const side_data* side = get_side_data(index);
			o[2 + i] = surface(side->primary_texture.texture, side->primary_transfer_mode,
							   light(side->primary_lightsource_index, side->ambient_delta));
		}
	}
}

// Patches around polygon p go back to being refreshed
void refresh_around(int p)
{
	auto refresh = [&](int patch) {
		if (patch >= 0)
			state.refresh[patch] = kRefreshBakes;
	};
	auto polygon_patches = [&](int q) {
		refresh(state.floor_patch[q]);
		refresh(state.ceiling_patch[q]);
		for (int i = 0; i < MAXIMUM_VERTICES_PER_POLYGON; ++i)
			refresh(state.wall_patch[size_t(q) * MAXIMUM_VERTICES_PER_POLYGON + i]);
	};
	polygon_patches(p);
	const polygon_data* polygon = get_polygon_data(p);
	for (int i = 0; i < polygon->vertex_count && i < MAXIMUM_VERTICES_PER_POLYGON; ++i)
		if (polygon->adjacent_polygon_indexes[i] != NONE)
			polygon_patches(polygon->adjacent_polygon_indexes[i]);
}

}

bool Frame(const view_data* view, const std::vector<sorted_node_data>& nodes, simd_float4 range, bool bounce)
{
	if (!state.ready || state.key != level_key() ||
		int(state.floor_patch.size()) != dynamic_world->polygon_count)
		build_layout();
	if (!state.ready || !DurandalMetal::RadianceReady())
		return false;

	// Moving platforms: what is around them changes
	for (size_t i = 0; i < PlatformList.size(); ++i)
	{
		const short p = PlatformList[i].polygon_index;
		if (p < 0 || p >= int(state.heights.size()))
			continue;
		const polygon_data* polygon = get_polygon_data(p);
		const simd_float2 now = simd_make_float2(polygon->floor_height, polygon->ceiling_height);
		if (now.x != state.heights[p].x || now.y != state.heights[p].y)
		{
			state.heights[p] = now;
			refresh_around(p);
		}
	}

	// Bake what is in view and not settled yet, nearest first
	// Why each patch is baked this frame, to undo its count if the bake
	// cannot run: converging (samples), refreshed near a platform that
	// moved (refresh owed), or refreshed for Bounced Light (nothing owed)
	enum Why { kConverging, kPlatform, kBounce };
	static std::vector<DurandalMetal::BakeTile> tiles;
	static std::vector<int> baked;
	static std::vector<Why> baked_why;
	tiles.clear();
	baked.clear();
	baked_why.clear();
	auto bake = [&](int patch) {
		if (patch < 0)
			return;
		const bool settled = state.samples[patch] >= kTarget;
		if (settled && state.refresh[patch] <= 0)
			return;
		const DurandalMetal::Patch& pa = state.patches[patch];
		const int across = (pa.rect.z + DurandalMetal::kBakeTile - 1) / DurandalMetal::kBakeTile;
		const int up = (pa.rect.w + DurandalMetal::kBakeTile - 1) / DurandalMetal::kBakeTile;
		if (!tiles.empty() && int(tiles.size()) + across * up > kTileBudget)
			return;
		// Converging: the running average; refreshed (near a platform that
		// moved): a gentle fixed blend, so the change comes in without noise
		const float blend = settled ? kRefreshBlend : float(kRays) / float(state.samples[patch] + kRays);
		for (int y = 0; y < up; ++y)
			for (int x = 0; x < across; ++x)
				tiles.push_back({ simd_make_int4(patch, x * DurandalMetal::kBakeTile, y * DurandalMetal::kBakeTile, 0),
								  simd_make_float4(blend, 0, 0, 0) });
		if (settled)
			state.refresh[patch]--;
		else
			state.samples[patch] += kRays;
		baked.push_back(patch);
		baked_why.push_back(settled ? kPlatform : kConverging);
	};
	for (auto it = nodes.rbegin(); it != nodes.rend() && int(tiles.size()) < kTileBudget; ++it)
	{
		const short p = it->polygon_index;
		if (p < 0 || p >= int(state.floor_patch.size()))
			continue;
		bake(state.floor_patch[p]);
		bake(state.ceiling_patch[p]);
		for (int i = 0; i < MAXIMUM_VERTICES_PER_POLYGON; ++i)
			bake(state.wall_patch[size_t(p) * MAXIMUM_VERTICES_PER_POLYGON + i]);
	}

	// Bounced Light: settled patches in view are baked again, a few tiles a
	// frame in turn, so what their neighbours gathered keeps flowing into
	// them (and a room whose lights change catches up). Round robin over
	// the nodes in view, so the far ones get their turn too.
	static std::vector<bool> refreshed;	// baked already this frame
	if (bounce && !nodes.empty())
	{
		refreshed.assign(state.patches.size(), false);
		for (int patch : baked)
			refreshed[patch] = true;
		const int budget = std::min(kTileBudget, int(tiles.size()) + kBounceTiles);
		auto again = [&](int patch) {
			if (patch < 0 || refreshed[patch] || state.samples[patch] < kTarget)
				return;
			const DurandalMetal::Patch& pa = state.patches[patch];
			const int across = (pa.rect.z + DurandalMetal::kBakeTile - 1) / DurandalMetal::kBakeTile;
			const int up = (pa.rect.w + DurandalMetal::kBakeTile - 1) / DurandalMetal::kBakeTile;
			const int total = across * up;
			const int room = budget - int(tiles.size());
			// A patch that fits waits for a frame with room for all of it; a
			// larger one than the whole allowance takes what is left, a
			// slice at a time
			if (room <= 0 || (total <= kBounceTiles && total > room))
				return;
			const int n = std::min(total, room);
			int& next = state.bounce_tile[patch];
			for (int k = 0; k < n; ++k)
			{
				const int tile = (next + k) % total;
				tiles.push_back({ simd_make_int4(patch, (tile % across) * DurandalMetal::kBakeTile,
												 (tile / across) * DurandalMetal::kBakeTile, 0),
								  simd_make_float4(kBounceBlend, 0, 0, 0) });
			}
			next = (next + n) % total;
			refreshed[patch] = true;
			baked.push_back(patch);
			baked_why.push_back(kBounce);
		};
		const size_t count = nodes.size();
		size_t visited = 0;
		for (; visited < count && int(tiles.size()) < budget; ++visited)
		{
			const short p = nodes[(state.bounce_cursor + visited) % count].polygon_index;
			if (p < 0 || p >= int(state.floor_patch.size()))
				continue;
			again(state.floor_patch[p]);
			again(state.ceiling_patch[p]);
			for (int i = 0; i < MAXIMUM_VERTICES_PER_POLYGON; ++i)
				again(state.wall_patch[size_t(p) * MAXIMUM_VERTICES_PER_POLYGON + i]);
		}
		state.bounce_cursor = (state.bounce_cursor + std::max<size_t>(visited, 1)) % count;
	}
	if (tiles.empty())
		return true;

	sky_from_landscape = bounce;
	static std::vector<simd_float4> surfaces;
	build_surfaces(surfaces);
	if (!DurandalMetal::RunRadiance(tiles, baked, surfaces, kRays, ++state.frame, bounce, range))
		for (size_t i = 0; i < baked.size(); ++i)
		{
			if (baked_why[i] == kPlatform)
				state.refresh[baked[i]]++;
			else if (baked_why[i] == kConverging)
				state.samples[baked[i]] -= kRays;
		}
	return true;
}

int FloorPatch(short polygon)
{
	return state.ready && polygon >= 0 && polygon < int(state.floor_patch.size()) ? state.floor_patch[polygon] : -1;
}

int CeilingPatch(short polygon)
{
	return state.ready && polygon >= 0 && polygon < int(state.ceiling_patch.size()) ? state.ceiling_patch[polygon] : -1;
}

int WallPatch(const side_texture_definition* texture)
{
	if (!state.ready || !texture || SideList.empty())
		return -1;
	const char* base = reinterpret_cast<const char*>(SideList.data());
	const char* at = reinterpret_cast<const char*>(texture);
	if (at < base)
		return -1;
	const size_t side = size_t(at - base) / sizeof(side_data);
	return side < state.side_patch.size() ? state.side_patch[side] : -1;
}

}
