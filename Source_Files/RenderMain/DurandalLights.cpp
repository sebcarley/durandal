/*
	DurandalLights.cpp — Durandal project

	See DurandalLights.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "cseries.h"
#include "DurandalLights.h"
#include "DurandalShading.h"

#include "map.h"
#include "render.h"
#include "RenderSortPoly.h"
#include "interface.h"
#include "collection_definition.h"
#include "textures.h"
#include "shape_descriptors.h"
#include "player.h"
#include "ChaseCam.h"
#include "lightsource.h"
#include "media.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace DurandalLights {

namespace {

// Reach at full strength, and how much a light at full strength adds to
// the surface light level
const float kRadius = 2.0f * WORLD_ONE;
const float kStrength = 0.9f;

const simd_float3 kWarm = { 1.0f, 0.85f, 0.6f };

// The colour a frame lights with: its pixels' colours weighted by their
// brightness squared, normalised so the strongest channel is 1
simd_float3 frame_colour(short collection_code, short low_level_shape)
{
	static uint32_t generation = 0;
	static std::unordered_map<uint32_t, simd_float3> cache;
	if (generation != DurandalShading::Generation())
	{
		cache.clear();
		generation = DurandalShading::Generation();
	}
	const uint32_t key = (uint32_t(uint16_t(collection_code)) << 16) | uint16_t(low_level_shape);
	auto it = cache.find(key);
	if (it != cache.end())
		return it->second;

	simd_float3 colour = kWarm;
	bitmap_definition* bitmap = nullptr;
	void* tables = nullptr;
	extended_get_shape_bitmap_and_shading_table(collection_code, low_level_shape, &bitmap, &tables, _shading_normal);
	const DurandalShading::Ramps* ramps = DurandalShading::Get(GET_COLLECTION(collection_code), GET_COLLECTION_CLUT(collection_code));
	if (bitmap && ramps)
	{
		double sum[3] = { 0, 0, 0 }, total = 0;
		auto add = [&](uint8 v) {
			if (!v)
				return;	// transparent
			const uint8_t* c = ramps->data[1][v];
			const double r = c[0] / 255.0, g = c[1] / 255.0, b = c[2] / 255.0;
			const double y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
			const double w = y * y;
			sum[0] += r * w; sum[1] += g * w; sum[2] += b * w;
			total += w;
		};
		const bool columns = (bitmap->flags & _COLUMN_ORDER_BIT) != 0;
		const int lines = columns ? bitmap->width : bitmap->height;
		const int length = columns ? bitmap->height : bitmap->width;
		for (int l = 0; l < lines; ++l)
		{
			const uint8* p = bitmap->row_addresses[l];
			if (bitmap->bytes_per_row != NONE)
			{
				for (int i = 0; i < length; ++i)
					add(p[i]);
			}
			else
			{
				// Marathon 2 sprite rows: first and last (big-endian), then pixels
				const int first = (p[0] << 8) | p[1], last = (p[2] << 8) | p[3];
				for (int i = 0; i < last - first; ++i)
					add(p[4 + i]);
			}
		}
		if (total > 0)
		{
			const double m = std::max({ sum[0], sum[1], sum[2] });
			if (m > 0)
				colour = simd_make_float3(sum[0] / m, sum[1] / m, sum[2] / m);
		}
	}
	cache[key] = colour;
	return colour;
}

}

int Gather(const view_data* view, const std::vector<sorted_node_data>& nodes, float weapon_flare,
		   DurandalMetal::Light* out)
{
	struct Candidate {
		DurandalMetal::Light light;
		float distance2;
	};
	std::vector<Candidate> found;
	world_point3d camera = view->origin;
	const simd_float3 eye = simd_make_float3(camera.x, camera.y, camera.z);

	auto add = [&](simd_float3 at, float strength, simd_float3 colour, short polygon) {
		DurandalMetal::Light l;
		l.position_radius = simd_make_float4(at.x, at.y, at.z, kRadius * (0.5f + 0.5f * strength));
		l.colour_strength = simd_make_float4(colour.x, colour.y, colour.z, kStrength * strength);
		// y: its size, for soft shadows (traced shadows, R1): an explosion is
		// larger than a bolt
		l.info = simd_make_int4(polygon, int(WORLD_ONE * (0.08f + 0.32f * std::min(strength, 1.0f))), 0, 0);
		const simd_float3 d = at - eye;
		found.push_back({ l, simd_dot(d, d) });
	};

	// The viewer's own muzzle flash
	if (weapon_flare > 0.05f)
		add(eye, weapon_flare * 0.6f, kWarm, view->origin_polygon_index);

	for (const sorted_node_data& node : nodes)
	{
		const polygon_data* polygon = get_polygon_data(node.polygon_index);
		if (!polygon)
			continue;
		for (short index = polygon->first_object; index != NONE; )
		{
			object_data* object = get_object_data(index);
			index = object->next_object;
			if (OBJECT_IS_INVISIBLE(object))
				continue;
			const int owner = GET_OBJECT_OWNER(object);
			if (owner != _object_is_projectile && owner != _object_is_effect && owner != _object_is_monster)
				continue;
			shape_and_transfer_mode data;
			get_object_shape_and_transfer_mode(&camera, object, &data);
			const shape_information_data* info = extended_get_shape_information(data.collection_code, data.low_level_shape_index);
			if (!info)
				continue;
			const float minimum = PIN(info->minimum_light_intensity, 0, FIXED_ONE) / float(FIXED_ONE);
			// Monsters light only on their firing frames
			if (minimum < (owner == _object_is_monster ? 0.5f : 0.1f))
				continue;
			const float height = (owner == _object_is_monster) ? 0.6f * WORLD_ONE : 0.0f;
			add(simd_make_float3(object->location.x, object->location.y, object->location.z + height), minimum,
				frame_colour(data.collection_code, data.low_level_shape_index), object->polygon);
		}
	}

	std::sort(found.begin(), found.end(), [](const Candidate& a, const Candidate& b) { return a.distance2 < b.distance2; });
	const int count = std::min<int>(int(found.size()), DurandalMetal::kMaximumLights);
	for (int i = 0; i < count; ++i)
		out[i] = found[i].light;
	return count;
}

int GatherCasters(const view_data* view, const std::vector<sorted_node_data>& nodes,
				  DurandalMetal::Caster* out)
{
	struct Candidate {
		DurandalMetal::Caster caster;
		float distance2;
	};
	std::vector<Candidate> found;
	world_point3d camera = view->origin;
	const short self = ChaseCam_IsActive() ? NONE : current_player->object_index;

	for (const sorted_node_data& node : nodes)
	{
		const polygon_data* polygon = get_polygon_data(node.polygon_index);
		if (!polygon)
			continue;
		for (short index = polygon->first_object; index != NONE; )
		{
			object_data* object = get_object_data(index);
			const short this_index = index;
			index = object->next_object;
			if (OBJECT_IS_INVISIBLE(object) || this_index == self)
				continue;
			float strength;
			switch (GET_OBJECT_OWNER(object))
			{
				case _object_is_item:		strength = 0.55f; break;
				case _object_is_monster:	strength = 0.5f; break;
				case _object_is_scenery:	strength = 0.45f; break;
				default:					continue;
			}
			shape_and_transfer_mode data;
			get_object_shape_and_transfer_mode(&camera, object, &data);
			const shape_information_data* info = extended_get_shape_information(data.collection_code, data.low_level_shape_index);
			if (!info)
				continue;
			// Self-lit frames (lamps, lava scenery, firing monsters) cast none
			if (info->minimum_light_intensity >= FIXED_ONE / 2)
				continue;
			// The floor under it; fading out as it rises (flying monsters)
			const polygon_data* under = get_polygon_data(object->polygon);
			if (!under)
				continue;
			const float floor = under->floor_height;
			const float height = object->location.z - floor;
			if (height < -16 || height > 1.5f * WORLD_ONE)
				continue;
			strength *= 1.0f - std::max(height, 0.0f) / (1.5f * WORLD_ONE);
			const float width = float(info->world_right - info->world_left);
			if (width <= 0)
				continue;
			const float radius = std::clamp(width * 0.6f, 80.0f, 0.75f * WORLD_ONE);

			DurandalMetal::Caster c;
			c.position_radius = simd_make_float4(object->location.x, object->location.y, floor, radius);
			c.info = simd_make_float4(strength, 0, 0, 0);
			const float dx = object->location.x - camera.x, dy = object->location.y - camera.y;
			found.push_back({ c, dx * dx + dy * dy });
		}
	}

	std::sort(found.begin(), found.end(), [](const Candidate& a, const Candidate& b) { return a.distance2 < b.distance2; });
	const int count = std::min<int>(int(found.size()), DurandalMetal::kMaximumCasters);
	for (int i = 0; i < count; ++i)
		out[i] = found[i].caster;
	return count;
}

int BuildMap(std::vector<simd_float4>& out)
{
	const int count = dynamic_world->polygon_count;
	out.resize(size_t(std::max(count, 1)) * 9);
	for (int p = 0; p < count; ++p)
	{
		const polygon_data* polygon = get_polygon_data(p);
		simd_float4* o = &out[size_t(p) * 9];
		const int n = std::min<int>(polygon->vertex_count, MAXIMUM_VERTICES_PER_POLYGON);
		for (int i = 0; i < 9; ++i)
			o[i] = simd_make_float4(0, 0, -1, 0);
		// Volumetric fog (V1) reads the spare w components: floor and
		// ceiling light, the liquid's height and type + 1
		const float light = 1.0f / float(FIXED_ONE);
		o[0] = simd_make_float4(n, polygon->floor_height, polygon->ceiling_height,
								get_light_intensity(polygon->floor_lightsource_index) * light);
		for (int i = 0; i < n; ++i)
		{
			const endpoint_data* e = get_endpoint_data(polygon->endpoint_indexes[i]);
			o[1 + i] = simd_make_float4(e->vertex.x, e->vertex.y, polygon->adjacent_polygon_indexes[i], 0);
		}
		o[1].w = get_light_intensity(polygon->ceiling_lightsource_index) * light;
		o[2].w = -1e9f;
		// Only liquid that is actually there: a drained liquid (below the
		// floor) must not glow or smoke in the fog
		if (polygon->media_index != NONE)
			if (const media_data* media = get_media_data(polygon->media_index); media && media->height > polygon->floor_height)
			{
				o[2].w = media->height;
				o[3].w = media->type + 1;
			}
	}
	return count;
}

}
