/*
	DurandalSurfaces.cpp — Durandal project

	See DurandalSurfaces.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "cseries.h"
#include "DurandalSurfaces.h"
#include "DurandalMetal.h"
#include "DurandalShading.h"

#include "map.h"
#include "render.h"
#include "interface.h"
#include "collection_definition.h"
#include "textures.h"
#include "shape_descriptors.h"
#include "lightsource.h"
#include "AnimatedTextures.h"
#include "ViewControl.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace DurandalSurfaces {

namespace {

using DurandalMetal::kColourSize;
using DurandalMetal::kColourSlices;

struct Cache {
	uint32_t generation = 0;
	std::unordered_map<uint16_t, int> slices;	// by shape descriptor
	int next_slice = 0;
	shape_descriptor sky = UNONE;				// the landscape in the sky texture
	simd_float4 sky_mapping = { 1, 0, 1, 0 };
};
Cache cache;

// A bitmap as RGBA through its colour table's ramps at full brightness,
// x across the bitmap (its columns), y down it; false without ramps
bool decode(shape_descriptor texture, std::vector<uint8_t>& rgba, int& w, int& h)
{
	const short code = GET_DESCRIPTOR_COLLECTION(texture), shape = GET_DESCRIPTOR_SHAPE(texture);
	bitmap_definition* bitmap = nullptr;
	void* tables = nullptr;
	extended_get_shape_bitmap_and_shading_table(code, shape, &bitmap, &tables, _shading_normal);
	const DurandalShading::Ramps* ramps = DurandalShading::Get(GET_COLLECTION(code), GET_COLLECTION_CLUT(code));
	if (!bitmap || !ramps || bitmap->width <= 0 || bitmap->height <= 0)
		return false;
	w = bitmap->width;
	h = bitmap->height;
	rgba.assign(size_t(w) * h * 4, 0);
	const bool columns = (bitmap->flags & _COLUMN_ORDER_BIT) != 0;
	const int lines = columns ? w : h;
	const int length = columns ? h : w;
	auto set = [&](int line, int i, uint8 v) {
		if (i < 0 || i >= length)
			return;
		const int x = columns ? line : i, y = columns ? i : line;
		const uint8_t* c = ramps->data[1][v];
		uint8_t* out = &rgba[(size_t(y) * w + x) * 4];
		out[0] = c[0]; out[1] = c[1]; out[2] = c[2]; out[3] = 255;
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
			const int first = (p[0] << 8) | p[1], last = (p[2] << 8) | p[3];
			for (int i = 0; i < last - first; ++i)
				set(l, first + i, p[4 + i]);
		}
	}
	return true;
}

// `rgba` (w x h) resampled to size x size (nearest), then each mip level
// down to 1x1 (box)
std::vector<std::vector<uint8_t>> levels_of(const std::vector<uint8_t>& rgba, int w, int h, int width, int height)
{
	std::vector<std::vector<uint8_t>> levels;
	levels.emplace_back(size_t(width) * height * 4);
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			const int sx = std::min(w - 1, x * w / width), sy = std::min(h - 1, y * h / height);
			std::memcpy(&levels[0][(size_t(y) * width + x) * 4], &rgba[(size_t(sy) * w + sx) * 4], 4);
		}
	int lw = width, lh = height;
	while (lw > 1 || lh > 1)
	{
		const int nw = std::max(1, lw / 2), nh = std::max(1, lh / 2);
		const std::vector<uint8_t>& above = levels.back();
		std::vector<uint8_t> level(size_t(nw) * nh * 4);
		for (int y = 0; y < nh; ++y)
			for (int x = 0; x < nw; ++x)
				for (int c = 0; c < 4; ++c)
				{
					const int x0 = std::min(lw - 1, x * 2), x1 = std::min(lw - 1, x * 2 + 1);
					const int y0 = std::min(lh - 1, y * 2), y1 = std::min(lh - 1, y * 2 + 1);
					const int sum = above[(size_t(y0) * lw + x0) * 4 + c] + above[(size_t(y0) * lw + x1) * 4 + c] +
						above[(size_t(y1) * lw + x0) * 4 + c] + above[(size_t(y1) * lw + x1) * 4 + c];
					level[(size_t(y) * nw + x) * 4 + c] = uint8_t((sum + 2) / 4);
				}
		levels.push_back(std::move(level));
		lw = nw;
		lh = nh;
	}
	return levels;
}

// The colour array slice for a wall texture (-1 if none can be made)
int slice_for(shape_descriptor texture)
{
	auto it = cache.slices.find(texture);
	if (it != cache.slices.end())
		return it->second;
	int slice = -1;
	std::vector<uint8_t> rgba;
	int w = 0, h = 0;
	if (cache.next_slice < kColourSlices && decode(texture, rgba, w, h) &&
		DurandalMetal::SetSurfaceColour(cache.next_slice, levels_of(rgba, w, h, kColourSize, kColourSize)))
		slice = cache.next_slice++;
	cache.slices[texture] = slice;
	return slice;
}

// The sky texture from the level's landscape, and how it wraps round the
// viewer: 2^HorizExp repeats in a circle, its azimuth, and the angle its
// height spans with square pixels in angle, the horizon at its middle
void place_sky(shape_descriptor texture)
{
	if (texture == cache.sky)
		return;
	cache.sky = texture;
	cache.sky_mapping = simd_make_float4(1, 0, 1, 0);
	std::vector<uint8_t> rgba;
	int w = 0, h = 0;
	if (!decode(texture, rgba, w, h))
		return;
	const int width = std::min(w, 1024), height = std::max(1, h * width / w);
	if (!DurandalMetal::SetSky(levels_of(rgba, w, h, width, height), width, height))
		return;
	const LandscapeOptions* opts = View_GetLandscapeOptions(texture);
	const float repeats = opts ? float(1 << std::clamp<int>(opts->HorizExp, 0, 8)) : 2.0f;
	const float azimuth = opts ? float(opts->Azimuth) / float(FULL_CIRCLE) : 0.0f;
	const float span = float(h) / float(w) * 2.0f * float(M_PI) / repeats;
	cache.sky_mapping = simd_make_float4(repeats, azimuth, span, 1);
}

float light(short index, int32 delta = 0)
{
	if (index == NONE)
		return 0.5f;
	return PIN(get_light_intensity(index) + delta, 0, FIXED_ONE) / float(FIXED_ONE);
}

simd_float4 flat(shape_descriptor texture, short transfer_mode, world_point2d origin, float brightness)
{
	if (transfer_mode == _xfer_landscape || transfer_mode == _xfer_big_landscape)
	{
		if (texture != UNONE)
			place_sky(AnimTxtr_Translate(texture));
		return simd_make_float4(-2, 0, 0, 1);
	}
	const int slice = texture == UNONE ? -1 : slice_for(AnimTxtr_Translate(texture));
	return simd_make_float4(slice, origin.x, origin.y, brightness);
}

simd_float4 side_part(const side_texture_definition& definition, short transfer_mode, short lightsource,
					  int32 ambient_delta, float top)
{
	if (transfer_mode == _xfer_landscape || transfer_mode == _xfer_big_landscape)
	{
		if (definition.texture != UNONE)
			place_sky(AnimTxtr_Translate(definition.texture));
		return simd_make_float4(-2, 0, 0, 1);
	}
	const int slice = definition.texture == UNONE ? -1 : slice_for(AnimTxtr_Translate(definition.texture));
	const float x0 = float(definition.x0 % WORLD_ONE), y0 = float(definition.y0 % WORLD_ONE);
	return simd_make_float4(slice, x0, top + y0, light(lightsource, ambient_delta));
}

}

bool Frame()
{
	const int polygons = dynamic_world->polygon_count;
	if (polygons <= 0)
		return false;
	if (cache.generation != DurandalShading::Generation())
	{
		cache = Cache();
		cache.generation = DurandalShading::Generation();
	}

	static std::vector<simd_float4> table;
	table.assign(1 + size_t(polygons) * kPerPolygon, simd_make_float4(-1, 0, 0, 0.5f));
	for (int p = 0; p < polygons; ++p)
	{
		const polygon_data* polygon = get_polygon_data(p);
		simd_float4* o = &table[1 + size_t(p) * kPerPolygon];
		const float floor_light = light(polygon->floor_lightsource_index);
		const float ceiling_light = light(polygon->ceiling_lightsource_index);
		o[0] = flat(polygon->floor_texture, polygon->floor_transfer_mode, polygon->floor_origin, floor_light);
		o[1] = flat(polygon->ceiling_texture, polygon->ceiling_transfer_mode, polygon->ceiling_origin, ceiling_light);
		const int n = std::min<int>(polygon->vertex_count, MAXIMUM_VERTICES_PER_POLYGON);
		for (int e = 0; e < n; ++e)
		{
			simd_float4& upper = o[2 + 2 * e];
			simd_float4& lower = o[3 + 2 * e];
			const short index = polygon->side_indexes[e];
			if (index == NONE || index >= dynamic_world->side_count)
			{
				upper = lower = simd_make_float4(-1, 0, 0, 0.5f * (floor_light + ceiling_light));
				continue;
			}
			const side_data* side = get_side_data(index);
			const line_data* line = get_line_data(polygon->line_indexes[e]);
			const float high_top = polygon->ceiling_height;
			const float low_top = std::max<float>(line->highest_adjacent_floor, polygon->floor_height);
			switch (side->type)
			{
				case _full_side:
					upper = lower = side_part(side->primary_texture, side->primary_transfer_mode,
											  side->primary_lightsource_index, side->ambient_delta, high_top);
					break;
				case _high_side:
					upper = side_part(side->primary_texture, side->primary_transfer_mode,
									  side->primary_lightsource_index, side->ambient_delta, high_top);
					lower = upper;
					break;
				case _low_side:
					lower = side_part(side->primary_texture, side->primary_transfer_mode,
									  side->primary_lightsource_index, side->ambient_delta, low_top);
					upper = lower;
					break;
				case _split_side:
					upper = side_part(side->primary_texture, side->primary_transfer_mode,
									  side->primary_lightsource_index, side->ambient_delta, high_top);
					lower = side_part(side->secondary_texture, side->secondary_transfer_mode,
									  side->secondary_lightsource_index, side->ambient_delta, low_top);
					break;
				default:
					break;
			}
		}
	}
	table[0] = cache.sky_mapping;
	DurandalMetal::SetSurfaces(table.data(), int(table.size()));
	return true;
}

}
