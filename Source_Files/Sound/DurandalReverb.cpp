/*
	DurandalReverb.cpp — Durandal project

	See DurandalReverb.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "DurandalReverb.h"

#include "cseries.h"
#include "DurandalPreferences.h"
#include "map.h"
#include "media.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <vector>

namespace DurandalReverb {

namespace {

const float kMetresPerUnit = 2.0f / WORLD_ONE;	// Marathon's world unit is about two metres
const float kReach = 24.0f * WORLD_ONE;			// how far round the listener the room is sized up
const int kMostPolygons = 400;
const world_distance kOpening = WORLD_ONE / 4;	// smaller gaps don't join rooms
const float kHardAbsorption = 0.1f;				// metal and stone
const float kSpeedOfSound = 343.0f;

float polygon_area(const polygon_data* polygon)
{
	double twice = 0;
	const int n = std::min<int>(polygon->vertex_count, MAXIMUM_VERTICES_PER_POLYGON);
	for (int i = 0; i < n; ++i)
	{
		const world_point2d a = get_endpoint_data(polygon->endpoint_indexes[i])->vertex;
		const world_point2d b = get_endpoint_data(polygon->endpoint_indexes[(i + 1) % n])->vertex;
		twice += double(a.x) * b.y - double(b.x) * a.y;
	}
	return float(std::fabs(twice) * 0.5);
}

world_point2d centre(const polygon_data* polygon)
{
	long x = 0, y = 0;
	const int n = std::max<int>(1, std::min<int>(polygon->vertex_count, MAXIMUM_VERTICES_PER_POLYGON));
	for (int i = 0; i < n; ++i)
	{
		const world_point2d v = get_endpoint_data(polygon->endpoint_indexes[i])->vertex;
		x += v.x;
		y += v.y;
	}
	return { world_distance(x / n), world_distance(y / n) };
}

bool landscape_side(short side_index)
{
	if (side_index == NONE)
		return false;
	const side_data* side = get_side_data(side_index);
	return side && side->primary_transfer_mode == _xfer_landscape;
}

Reverb underwater(const media_data* media)
{
	// EFX_REVERB_PRESET_UNDERWATER, darker for thicker liquids
	Reverb r;
	r.active = true;
	r.underwater = true;
	r.density = 0.3645f;
	r.diffusion = 1.0f;
	r.gain = 0.3162f;
	r.gain_hf = (media && media->type != _media_water && media->type != _media_jjaro) ? 0.004f : 0.01f;
	r.decay = 1.49f;
	r.decay_hf_ratio = 0.1f;
	r.reflections_gain = 0.5963f;
	r.reflections_delay = 0.007f;
	r.late_gain = 7.0795f;
	r.late_delay = 0.011f;
	r.air_absorption_hf = 0.994f;
	return r;
}

}

Reverb Estimate(const world_location3d& listener)
{
	Reverb r;
	if (!Durandal::Enabled(Durandal::kReverb))
		return r;
	const short start = listener.polygon_index;
	if (start < 0 || start >= dynamic_world->polygon_count)
		return r;

	const polygon_data* here = get_polygon_data(start);
	if (here->media_index != NONE)
	{
		const media_data* media = get_media_data(here->media_index);
		if (media && listener.point.z < media->height)
			return underwater(media);
	}

	// Walk the room: breadth first through openings, by distance
	static std::vector<char> seen;
	seen.assign(dynamic_world->polygon_count, 0);
	struct Step { short polygon; float distance; };
	std::queue<Step> queue;
	queue.push({ start, 0.0f });
	seen[start] = 1;
	double volume = 0, hard = 0, open = 0;
	int count = 0;
	while (!queue.empty() && count < kMostPolygons)
	{
		const Step step = queue.front();
		queue.pop();
		++count;
		const polygon_data* polygon = get_polygon_data(step.polygon);
		const float height = std::max<float>(polygon->ceiling_height - polygon->floor_height, 0);
		const float area = polygon_area(polygon);
		volume += double(area) * height;
		hard += area;	// the floor (liquids reflect too)
		(polygon->ceiling_transfer_mode == _xfer_landscape ? open : hard) += area;

		const world_point2d here_centre = centre(polygon);
		const int n = std::min<int>(polygon->vertex_count, MAXIMUM_VERTICES_PER_POLYGON);
		for (int i = 0; i < n; ++i)
		{
			const world_point2d a = get_endpoint_data(polygon->endpoint_indexes[i])->vertex;
			const world_point2d b = get_endpoint_data(polygon->endpoint_indexes[(i + 1) % n])->vertex;
			const float length = std::hypot(float(b.x - a.x), float(b.y - a.y));
			const bool sky = landscape_side(polygon->side_indexes[i]);
			const short beyond = polygon->adjacent_polygon_indexes[i];
			float gap = 0;
			if (beyond != NONE)
			{
				const polygon_data* other = get_polygon_data(beyond);
				gap = std::max<float>(0, std::min(polygon->ceiling_height, other->ceiling_height) -
										 std::max(polygon->floor_height, other->floor_height));
				if (gap < kOpening)
					gap = 0;
			}
			// The wall above and below the opening
			(sky ? open : hard) += double(length) * std::max(height - gap, 0.0f);
			if (gap <= 0 || seen[beyond])
				continue;
			const float distance = step.distance + std::hypot(float(centre(get_polygon_data(beyond)).x - here_centre.x),
															   float(centre(get_polygon_data(beyond)).y - here_centre.y));
			if (distance > kReach)
			{
				// Sound leaks on beyond the reach: half of it comes back
				open += 0.5 * length * gap;
				continue;
			}
			seen[beyond] = 1;
			queue.push({ beyond, distance });
		}
	}

	const double m = kMetresPerUnit;
	const double v = volume * m * m * m;
	const double s_hard = hard * m * m, s_open = open * m * m;
	const double s = s_hard + s_open;
	if (v <= 0 || s <= 0)
		return r;
	const double absorption = s_hard * kHardAbsorption + s_open;
	const float rt60 = float(std::clamp(0.161 * v / absorption, 0.15, 6.0));
	const float free_path = float(4.0 * v / s);
	const float openness = float(s_open / s);

	r.active = true;
	r.decay = std::clamp(rt60, 0.2f, 5.0f);
	r.density = std::clamp(float(v / 800.0), 0.2f, 1.0f);
	r.diffusion = 1.0f - 0.4f * openness;
	r.gain = 0.32f;
	r.gain_hf = 0.89f;
	r.decay_hf_ratio = 0.83f;
	r.reflections_delay = std::clamp(free_path / kSpeedOfSound, 0.002f, 0.3f);
	r.late_delay = std::clamp(1.5f * free_path / kSpeedOfSound, 0.005f, 0.1f);
	r.reflections_gain = std::clamp((1.0f - openness) * (1.1f - free_path / 30.0f), 0.05f, 1.0f);
	r.late_gain = std::clamp((1.0f - 0.8f * openness) * (0.5f + 0.9f * std::min(rt60 / 2.5f, 1.0f)), 0.1f, 1.6f);
	return r;
}

Reverb Glide(const Reverb& current, const Reverb& target, float seconds)
{
	if (!current.active || current.underwater != target.underwater || !target.active)
		return target;
	const float k = std::clamp(seconds / 0.4f, 0.0f, 1.0f);
	auto to = [k](float a, float b) { return a + (b - a) * k; };
	Reverb r = target;
	r.density = to(current.density, target.density);
	r.diffusion = to(current.diffusion, target.diffusion);
	r.gain = to(current.gain, target.gain);
	r.gain_hf = to(current.gain_hf, target.gain_hf);
	r.decay = to(current.decay, target.decay);
	r.decay_hf_ratio = to(current.decay_hf_ratio, target.decay_hf_ratio);
	r.reflections_gain = to(current.reflections_gain, target.reflections_gain);
	r.reflections_delay = to(current.reflections_delay, target.reflections_delay);
	r.late_gain = to(current.late_gain, target.late_gain);
	r.late_delay = to(current.late_delay, target.late_delay);
	r.air_absorption_hf = to(current.air_absorption_hf, target.air_absorption_hf);
	return r;
}

}
