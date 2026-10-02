/*
	DurandalOccluders.cpp — Durandal project

	See DurandalOccluders.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "cseries.h"
#include "DurandalOccluders.h"
#include "DurandalShading.h"

#include "map.h"
#include "render.h"
#include "interface.h"
#include "collection_definition.h"
#include "textures.h"
#include "shape_descriptors.h"
#include "player.h"
#include "ChaseCam.h"

#include "lightsource.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace DurandalOccluders {

namespace {

using DurandalMetal::kMaskSize;
using DurandalMetal::kMaskSlices;

// A frame's silhouette in the mask array
struct Mask {
	int slice;
	int width, height;		// in the slice, texels (the bitmap, shrunk to fit if larger)
	uint32_t last;			// the frame it was last used
};

struct Cache {
	uint32_t generation = 0;
	uint32_t frame = 0;
	std::unordered_map<uint32_t, Mask> masks;	// by collection code and low-level shape
	std::vector<uint32_t> slice_key;			// per slice: its key, or none
	std::vector<bool> slice_used;
};
Cache cache;

// Rasterises a frame's opacity into a free (or long unused) slice
const Mask* mask_for(short collection_code, short low_level_shape)
{
	const uint32_t key = (uint32_t(uint16_t(collection_code)) << 16) | uint16_t(low_level_shape);
	auto it = cache.masks.find(key);
	if (it != cache.masks.end())
	{
		it->second.last = cache.frame;
		return &it->second;
	}

	bitmap_definition* bitmap = nullptr;
	void* tables = nullptr;
	extended_get_shape_bitmap_and_shading_table(collection_code, low_level_shape, &bitmap, &tables, _shading_normal);
	if (!bitmap || bitmap->width <= 0 || bitmap->height <= 0)
		return nullptr;

	// A slice: a free one, else the one unused longest (and not in the last
	// few frames, which the GPU may still be reading)
	int slice = -1;
	for (int s = 0; s < kMaskSlices && slice < 0; ++s)
		if (!cache.slice_used[s])
			slice = s;
	if (slice < 0)
	{
		uint32_t oldest = cache.frame;
		for (auto& [k, m] : cache.masks)
			if (m.last + 3 < cache.frame && m.last < oldest)
			{
				oldest = m.last;
				slice = m.slice;
			}
		if (slice < 0)
			return nullptr;
		cache.masks.erase(cache.slice_key[slice]);
	}

	// The bitmap's colour at full brightness and its opacity (colour 0 is
	// clear in a transparent bitmap): reflections draw the figure, shadows
	// use its opacity
	const int w = bitmap->width, h = bitmap->height;
	std::vector<uint8_t> grid(size_t(w) * h * 4, 0);
	const bool transparent = (bitmap->flags & _TRANSPARENT_BIT) != 0;
	const DurandalShading::Ramps* ramps = DurandalShading::Get(GET_COLLECTION(collection_code), GET_COLLECTION_CLUT(collection_code));
	const bool columns = (bitmap->flags & _COLUMN_ORDER_BIT) != 0;
	const int lines = columns ? w : h;
	const int length = columns ? h : w;
	auto set = [&](int line, int i, uint8 v) {
		if (i < 0 || i >= length)
			return;
		const int x = columns ? line : i, y = columns ? i : line;
		uint8_t* out = &grid[(size_t(y) * w + x) * 4];
		if (ramps)
		{
			const uint8_t* c = ramps->data[1][v];
			out[0] = c[0]; out[1] = c[1]; out[2] = c[2];
		}
		else
			out[0] = out[1] = out[2] = 128;
		out[3] = (!transparent || v) ? 255 : 0;
	};
	for (int l = 0; l < lines; ++l)
	{
		const uint8* p = bitmap->row_addresses[l];
		if (!p)
			continue;
		if (bitmap->bytes_per_row != NONE)
		{
			for (int i = 0; i < length; ++i)
				set(l, i, p[i]);
		}
		else
		{
			// Marathon 2 sprite columns: first and last (big-endian), then pixels
			const int first = (p[0] << 8) | p[1], last = (p[2] << 8) | p[3];
			for (int i = 0; i < last - first; ++i)
				set(l, first + i, p[4 + i]);
		}
	}

	// Into the slice at its top left, shrunk to fit if larger; the rest of
	// the slice stays clear, so blurred edges fade into nothing
	const float scale = std::min(1.0f, float(kMaskSize) / float(std::max(w, h)));
	const int mw = std::max(1, int(std::ceil(w * scale))), mh = std::max(1, int(std::ceil(h * scale)));
	std::vector<std::vector<uint8_t>> levels;
	levels.emplace_back(size_t(kMaskSize) * kMaskSize * 4, 0);
	for (int y = 0; y < mh; ++y)
		for (int x = 0; x < mw; ++x)
		{
			const int sx = std::min(w - 1, int((x + 0.5f) / scale)), sy = std::min(h - 1, int((y + 0.5f) / scale));
			std::memcpy(&levels[0][(size_t(y) * kMaskSize + x) * 4], &grid[(size_t(sy) * w + sx) * 4], 4);
		}
	// Mips: colour weighted by opacity (clear texels add no colour), then
	// opacity averaged
	for (int size = kMaskSize / 2; size >= 1; size /= 2)
	{
		const std::vector<uint8_t>& above = levels.back();
		std::vector<uint8_t> level(size_t(size) * size * 4);
		const int a = size * 2;
		for (int y = 0; y < size; ++y)
			for (int x = 0; x < size; ++x)
			{
				int rgb[3] = { 0, 0, 0 }, alpha = 0;
				for (int k = 0; k < 4; ++k)
				{
					const uint8_t* t = &above[(size_t(y * 2 + k / 2) * a + x * 2 + k % 2) * 4];
					for (int c = 0; c < 3; ++c)
						rgb[c] += t[c] * t[3];
					alpha += t[3];
				}
				uint8_t* out = &level[(size_t(y) * size + x) * 4];
				for (int c = 0; c < 3; ++c)
					out[c] = alpha ? uint8_t(rgb[c] / alpha) : 0;
				out[3] = uint8_t((alpha + 2) / 4);
			}
		levels.push_back(std::move(level));
	}
	if (!DurandalMetal::SetMask(slice, levels))
		return nullptr;

	cache.slice_used[slice] = true;
	cache.slice_key[slice] = key;
	Mask& m = cache.masks[key];
	m = { slice, mw, mh, cache.frame };
	return &m;
}

}

int Gather(const view_data* view, const DurandalMetal::Light* light_list, int light_count, bool around_viewer)
{
	static std::vector<DurandalMetal::Occluder> found;
	static std::vector<float> distance2;
	static std::vector<std::vector<int>> per_polygon;
	static std::vector<simd_int2> lists;
	static std::vector<int> indices;

	const int polygons = dynamic_world->polygon_count;
	if ((light_count <= 0 && !around_viewer) || polygons <= 0)
	{
		DurandalMetal::SetOccluders(nullptr, 0, nullptr, 0, nullptr, 0);
		return 0;
	}

	// A new colour environment (a level change) reloads the bitmaps
	if (cache.generation != DurandalShading::Generation() || cache.slice_key.empty())
	{
		cache.generation = DurandalShading::Generation();
		cache.masks.clear();
		cache.slice_key.assign(kMaskSlices, 0);
		cache.slice_used.assign(kMaskSlices, false);
	}
	++cache.frame;

	world_point3d camera = view->origin;
	const short self = ChaseCam_IsActive() ? NONE : current_player->object_index;

	// Figures within reach of a light, nearest the viewer first
	struct Candidate {
		short index;
		float distance2;
	};
	static std::vector<Candidate> candidates;
	candidates.clear();
	for (size_t slot = 0; slot < ObjectList.size(); ++slot)
	{
		const short i = short(slot);
		object_data* object = &ObjectList[slot];
		if (!SLOT_IS_USED(object) || OBJECT_IS_INVISIBLE(object) || i == self)
			continue;
		switch (GET_OBJECT_OWNER(object))
		{
			case _object_is_monster:
			case _object_is_item:
			case _object_is_scenery:
			case _object_is_garbage:
				break;
			default:
				continue;
		}
		if (object->polygon < 0 || object->polygon >= polygons)
			continue;
		bool near = false;
		if (around_viewer)
		{
			const float dx = object->location.x - camera.x, dy = object->location.y - camera.y;
			near = dx * dx + dy * dy < 12.0f * WORLD_ONE * 12.0f * WORLD_ONE;
		}
		for (int l = 0; l < light_count && !near; ++l)
		{
			const simd_float4 p = light_list[l].position_radius;
			const float dx = object->location.x - p.x, dy = object->location.y - p.y;
			const float reach = p.w + WORLD_ONE;
			near = dx * dx + dy * dy < reach * reach;
		}
		if (!near)
			continue;
		const float dx = object->location.x - camera.x, dy = object->location.y - camera.y;
		candidates.push_back({ i, dx * dx + dy * dy });
	}
	std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.distance2 < b.distance2; });

	found.clear();
	if (per_polygon.size() < size_t(polygons))
		per_polygon.resize(polygons);
	for (int p = 0; p < polygons; ++p)
		per_polygon[p].clear();
	for (const Candidate& c : candidates)
	{
		if (int(found.size()) >= DurandalMetal::kMaximumOccluders)
			break;
		object_data* object = &ObjectList[c.index];
		shape_and_transfer_mode data;
		get_object_shape_and_transfer_mode(&camera, object, &data);
		// Cloaked, teleporting or static figures throw no shadow
		if (data.transfer_mode != _xfer_normal && data.transfer_mode != _xfer_fade_out_to_black)
			continue;
		const shape_information_data* info = extended_get_shape_information(data.collection_code, data.low_level_shape_index);
		if (!info)
			continue;
		// Self-lit frames (lamps, lava scenery, firing monsters) are lights, not shadows
		if (info->minimum_light_intensity >= FIXED_ONE / 2)
			continue;
		const float left = info->world_left, right = info->world_right;
		const float bottom = info->world_bottom, top = info->world_top;
		if (right - left <= 0 || top - bottom <= 0)
			continue;
		const Mask* mask = mask_for(data.collection_code, data.low_level_shape_index);
		if (!mask)
			continue;

		DurandalMetal::Occluder o;
		o.position = simd_make_float4(object->location.x, object->location.y, object->location.z, 0);
		o.extent = simd_make_float4(left, right, bottom, top);
		o.mask = simd_make_float4(float(mask->width) / kMaskSize, float(mask->height) / kMaskSize,
								  float(mask->width) / (right - left), 0);
		// z: its light (the floor's, or its own minimum), thousandths, for reflections
		const polygon_data* under = get_polygon_data(object->polygon);
		const int32 own = std::max<int32>(get_light_intensity(under->floor_lightsource_index), info->minimum_light_intensity);
		o.info = simd_make_int4(mask->slice, (info->flags & _X_MIRRORED_BIT) ? 1 : 0,
								int(1000.0f * PIN(own, 0, FIXED_ONE) / float(FIXED_ONE)), 0);
		const int n = int(found.size());
		found.push_back(o);

		// A ray meets the card within its half-width of the figure, so it
		// may be crossing the figure's polygon or one beside it
		const polygon_data* polygon = get_polygon_data(object->polygon);
		per_polygon[object->polygon].push_back(n);
		for (int e = 0; e < polygon->vertex_count && e < MAXIMUM_VERTICES_PER_POLYGON; ++e)
		{
			const short across = polygon->adjacent_polygon_indexes[e];
			if (across >= 0 && across < polygons)
				per_polygon[across].push_back(n);
		}
	}
	if (found.empty())
	{
		DurandalMetal::SetOccluders(nullptr, 0, nullptr, 0, nullptr, 0);
		return 0;
	}

	lists.assign(polygons, simd_make_int2(0, 0));
	indices.clear();
	for (int p = 0; p < polygons; ++p)
	{
		lists[p] = simd_make_int2(int(indices.size()), int(per_polygon[p].size()));
		indices.insert(indices.end(), per_polygon[p].begin(), per_polygon[p].end());
	}
	DurandalMetal::SetOccluders(found.data(), int(found.size()), lists.data(), polygons, indices.data(), int(indices.size()));
	// Development: DURANDAL_OCCLUDER_LOG=1 prints, each frame, the figures
	// gathered, the longest polygon list and how many of them are garbage
	static const bool log = getenv("DURANDAL_OCCLUDER_LOG") != nullptr;
	if (log)
	{
		size_t longest = 0;
		for (int p = 0; p < polygons; ++p)
			longest = std::max(longest, per_polygon[p].size());
		int garbage = 0;
		for (const Candidate& c : candidates)
			garbage += GET_OBJECT_OWNER(&ObjectList[c.index]) == _object_is_garbage ? 1 : 0;
		fprintf(stderr, "Durandal occluders: tick %d, %zu figures (%zu candidates, %d garbage), longest polygon list %zu\n",
				int(view->tick_count), found.size(), candidates.size(), garbage, longest);
	}
	return int(found.size());
}

}
