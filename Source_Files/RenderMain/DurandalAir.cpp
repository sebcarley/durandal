/*
	DurandalAir.cpp — Durandal project

	See DurandalAir.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "cseries.h"
#include "DurandalAir.h"

#include "map.h"
#include "render.h"
#include "RenderSortPoly.h"
#include "lightsource.h"
#include "media.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace DurandalAir {

namespace {

uint32_t hash(uint32_t a, uint32_t b)
{
	uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
	h ^= h >> 16;
	h *= 0x85EBCA6Bu;
	h ^= h >> 13;
	h *= 0xC2B2AE35u;
	h ^= h >> 16;
	return h;
}

float unit(uint32_t h)
{
	return float(h & 0xFFFFFFu) / 16777216.0f;
}

float fract(double x)
{
	return float(x - std::floor(x));
}

bool inside(const polygon_data* polygon, float x, float y)
{
	const int n = std::min<int>(polygon->vertex_count, MAXIMUM_VERTICES_PER_POLYGON);
	float sign = 0;
	for (int i = 0; i < n; ++i)
	{
		const world_point2d a = get_endpoint_data(polygon->endpoint_indexes[i])->vertex;
		const world_point2d b = get_endpoint_data(polygon->endpoint_indexes[(i + 1) % n])->vertex;
		const float c = float(b.x - a.x) * (y - a.y) - float(b.y - a.y) * (x - a.x);
		if (sign == 0)
			sign = c;
		else if (c * sign < 0)
			return false;
	}
	return true;
}

// The dynamic lights at a point: their colour x strength, summed
simd_float3 lights_at(simd_float3 p, const DurandalMetal::Light* list, int count)
{
	simd_float3 sum = simd_make_float3(0, 0, 0);
	for (int i = 0; i < count; ++i)
	{
		const simd_float4 l = list[i].position_radius;
		const simd_float3 d = l.xyz - p;
		const float d2 = simd_dot(d, d);
		if (d2 >= l.w * l.w)
			continue;
		float f = 1.0f - d2 / (l.w * l.w);
		f *= f;
		sum += list[i].colour_strength.xyz * list[i].colour_strength.w * f;
	}
	return sum;
}

}

int Build(const view_data* view, const std::vector<sorted_node_data>& nodes, const DurandalMetal::Light* light_list,
		  int light_count, std::vector<DurandalMetal::Mote>& out)
{
	out.clear();
	const double time = (double(view->tick_count) + view->heartbeat_fraction) / TICKS_PER_SECOND;
	const simd_float3 eye = simd_make_float3(view->origin.x, view->origin.y, view->origin.z);
	const float reach = 16.0f * WORLD_ONE;	// motes further off are below a pixel

	for (const sorted_node_data& node : nodes)
	{
		if (int(out.size()) >= DurandalMetal::kMaximumMotes)
			break;
		const short p = node.polygon_index;
		if (p < 0 || p >= dynamic_world->polygon_count)
			continue;
		const polygon_data* polygon = get_polygon_data(p);
		const int n = std::min<int>(polygon->vertex_count, MAXIMUM_VERTICES_PER_POLYGON);
		if (n < 3 || polygon->ceiling_height <= polygon->floor_height)
			continue;
		float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
		for (int i = 0; i < n; ++i)
		{
			const world_point2d v = get_endpoint_data(polygon->endpoint_indexes[i])->vertex;
			x0 = std::min<float>(x0, v.x); y0 = std::min<float>(y0, v.y);
			x1 = std::max<float>(x1, v.x); y1 = std::max<float>(y1, v.y);
		}
		const float w = std::max(x1 - x0, 1.0f), d = std::max(y1 - y0, 1.0f);
		const float floor = polygon->floor_height, ceiling = polygon->ceiling_height;
		const float height = ceiling - floor;
		const float area = w * d / float(WORLD_ONE * WORLD_ONE);	// world units squared, roughly
		const float room = PIN(get_light_intensity(polygon->floor_lightsource_index), 0, FIXED_ONE) / float(FIXED_ONE);
		const bool open_sky = polygon->ceiling_transfer_mode == _xfer_landscape || polygon->ceiling_transfer_mode == _xfer_big_landscape;
		float lava = -1;
		if (polygon->media_index != NONE)
			if (const media_data* media = get_media_data(polygon->media_index))
				if (media->type == _media_lava && media->height > floor)
					lava = media->height;

		// Dust: drifting slowly, seen where light falls on it
		const int dust = std::min(48, int(area * (open_sky ? 6.0f : 3.0f)));
		for (int i = 0; i < dust && int(out.size()) < DurandalMetal::kMaximumMotes; ++i)
		{
			const uint32_t h = hash(uint32_t(p), uint32_t(i));
			const float vx = (unit(hash(h, 4)) - 0.5f) * 60.0f, vy = (unit(hash(h, 5)) - 0.5f) * 60.0f;	// units per second
			const float vz = (unit(hash(h, 6)) - 0.6f) * 25.0f;
			const float phase = unit(hash(h, 7)) * 6.2831853f;
			const float x = x0 + fract(unit(hash(h, 1)) + time * vx / w) * w + std::sin(float(time) * 0.7f + phase) * 20.0f;
			const float y = y0 + fract(unit(hash(h, 2)) + time * vy / d) * d + std::cos(float(time) * 0.6f + phase) * 20.0f;
			const float z = floor + fract(unit(hash(h, 3)) + time * vz / height) * height;
			if (lava >= 0 && z < lava)
				continue;
			if (!inside(polygon, x, y))
				continue;
			const simd_float3 at = simd_make_float3(x, y, z);
			if (simd_length(at - eye) > reach)
				continue;
			// Size and light set so a mote in a lit room is a faint speck a
			// few pixels across at a few world units (smaller, dimmer ones
			// vanished) and one by a bolt glints
			const simd_float3 lit = simd_make_float3(0.95f, 0.9f, 0.8f) * (room * 0.3f) + lights_at(at, light_list, light_count) * 1.5f;
			const float brightness = std::max({ lit.x, lit.y, lit.z });
			if (brightness < 0.04f)
				continue;
			DurandalMetal::Mote m;
			m.position_size = simd_make_float4(x, y, z, 6.0f + 4.0f * unit(hash(h, 8)));
			m.colour = simd_make_float4(lit.x, lit.y, lit.z, 0.7f);
			m.info = simd_make_float4(0, 0, 0, 0);
			out.push_back(m);
		}

		// Embers: rising off lava, glowing, fading as they climb
		if (lava >= 0)
		{
			const int embers = std::min(48, int(area * 12.0f) + 6);
			for (int i = 0; i < embers && int(out.size()) < DurandalMetal::kMaximumMotes; ++i)
			{
				const uint32_t h = hash(uint32_t(p) + 0x51ED2701u, uint32_t(i));
				const float speed = 300.0f + 300.0f * unit(hash(h, 3));	// units per second
				const float climb = 1.5f * WORLD_ONE;
				const float life = fract(unit(hash(h, 4)) + time * speed / climb);
				const float phase = unit(hash(h, 5)) * 6.2831853f;
				const float x = x0 + unit(hash(h, 1)) * w + std::sin(float(time) * 1.3f + phase) * 40.0f * life;
				const float y = y0 + unit(hash(h, 2)) * d + std::cos(float(time) * 1.1f + phase) * 40.0f * life;
				const float z = lava + life * climb;
				if (z > ceiling || !inside(polygon, x, y))
					continue;
				const simd_float3 at = simd_make_float3(x, y, z);
				if (simd_length(at - eye) > reach)
					continue;
				const float fade = std::pow(1.0f - life, 1.5f);
				DurandalMetal::Mote m;
				// Big and bright enough to read as sparks a few world units off
				// (half the size came out as one-pixel specks); they glow, so
				// they bloom
				m.position_size = simd_make_float4(x, y, z, 20.0f + 10.0f * unit(hash(h, 6)));
				// Half as bright and half the glow as first built (the owner's
				// first look, 1 Oct 2026: too strong)
				m.colour = simd_make_float4(1.0f * fade, 0.425f * fade, 0.1f * fade, 1.0f);
				m.info = simd_make_float4(0.75f, 0, 0, 0);
				out.push_back(m);
			}
		}
	}
	// Development: DURANDAL_AIR_LOG=1 prints the count now and then; 2 also
	// draws every mote large and white
	static const bool log = getenv("DURANDAL_AIR_LOG") != nullptr;
	static const bool loud = log && std::atoi(getenv("DURANDAL_AIR_LOG")) == 2;
	if (loud)
		for (DurandalMetal::Mote& m : out)
		{
			m.position_size.w = 60;
			m.colour = simd_make_float4(1, 1, 1, 1);
		}
	static int frames = 0;
	if (log && (frames++ % 120) == 0)
	{
		size_t embers = 0;
		for (const DurandalMetal::Mote& m : out)
			embers += m.info.x > 0 ? 1 : 0;
		fprintf(stderr, "Durandal air: %zu motes (%zu embers), %zu nodes, tick %d\n", out.size(), embers, nodes.size(), int(view->tick_count));
	}
	return int(out.size());
}

}
