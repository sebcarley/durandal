/*
	DurandalAcoustics.cpp — Durandal project

	See DurandalAcoustics.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "DurandalAcoustics.h"

#include "cseries.h"
#include "DurandalPreferences.h"
#include "interface.h"
#include "map.h"
#include "player.h"
#include "render.h"
#include "SoundManager.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <vector>

extern struct view_data* world_view;

namespace Durandal {

namespace {

// Sounds come from an object's middle, not its feet.
const world_distance kSourceLift = WORLD_ONE / 2;
// A ray this close to an opening's edge still counts as passing through.
const world_distance kEdgeTolerance = WORLD_ONE / 16;
// Openings smaller than this (a door almost shut) don't carry sound round corners.
const world_distance kMinimumOpening = WORLD_ONE / 8;
// How far round corners we look, and the detour that reaches full occlusion.
const float kMaximumPath = 16.f * WORLD_ONE;
const float kFullDetour = 4.f * WORLD_ONE;
// Occlusion per ledge or step the direct ray passes below or above, and
// the range used for sound that has to go round.
const float kPerHeightBlock = 0.35f;
const float kMaximumHeightOcclusion = 0.8f;
const float kRoundCornerMinimum = 0.35f;
const float kRoundCornerMaximum = 0.9f;

struct DirectResult {
	bool solid;			// a wall, or the walk lost its way
	int height_blocks;	// openings the ray passed above or below
};

float distance2d(const world_point2d& a, const world_point2d& b)
{
	return std::hypot(float(a.x - b.x), float(a.y - b.y));
}

// Where along p0->p1 (0..1) it crosses the line e0-e1.
float crossing_fraction(const world_point2d& p0, const world_point2d& p1, const world_point2d& e0, const world_point2d& e1)
{
	const float dx = p1.x - p0.x, dy = p1.y - p0.y;
	const float ex = e1.x - e0.x, ey = e1.y - e0.y;
	const float denominator = dx * ey - dy * ex;
	if (std::fabs(denominator) < 1e-6f)
		return 0.5f;
	const float t = ((e0.x - p0.x) * ey - (e0.y - p0.y) * ex) / denominator;
	return std::clamp(t, 0.f, 1.f);
}

// The same polygon walk as line_is_obstructed(), plus height.
DirectResult walk(short polygon_index, const world_point3d& a, short target_polygon, const world_point3d& b)
{
	world_point2d p0 = {a.x, a.y}, p1 = {b.x, b.y};
	DirectResult result = {false, 0};

	for (int guard = 0; guard < 256; ++guard)
	{
		const short line_index = find_line_crossed_leaving_polygon(polygon_index, &p0, &p1);
		if (line_index == NONE)
		{
			result.solid = polygon_index != target_polygon;
			return result;
		}

		line_data* line = get_line_data(line_index);
		if (LINE_IS_SOLID(line) && !LINE_HAS_TRANSPARENT_SIDE(line))
		{
			result.solid = true;
			return result;
		}

		const short next = find_adjacent_polygon(polygon_index, line_index);
		if (next == NONE)
		{
			result.solid = true;
			return result;
		}

		const world_point2d& e0 = get_endpoint_data(line->endpoint_indexes[0])->vertex;
		const world_point2d& e1 = get_endpoint_data(line->endpoint_indexes[1])->vertex;
		const float t = crossing_fraction(p0, p1, e0, e1);
		const float z = a.z + (b.z - a.z) * t;
		if (z < line->highest_adjacent_floor - kEdgeTolerance || z > line->lowest_adjacent_ceiling + kEdgeTolerance)
			++result.height_blocks;

		polygon_index = next;
	}

	result.solid = true;
	return result;
}

// Shortest distances from the listener's polygon to every polygon within
// kMaximumPath, through openings sound can pass. Rebuilt once per tick
// and whenever the listener changes polygon.
struct PathMap {
	int32 tick = -1;
	short origin = NONE;
	int16 polygon_count = 0;
	std::vector<float> distance;
};

PathMap path_map;

const std::vector<float>& paths_from(short origin)
{
	if (path_map.tick == dynamic_world->tick_count && path_map.origin == origin &&
		path_map.polygon_count == dynamic_world->polygon_count)
		return path_map.distance;

	path_map.tick = dynamic_world->tick_count;
	path_map.origin = origin;
	path_map.polygon_count = dynamic_world->polygon_count;
	path_map.distance.assign(dynamic_world->polygon_count, std::numeric_limits<float>::infinity());

	using Entry = std::pair<float, short>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
	path_map.distance[origin] = 0;
	queue.push({0.f, origin});

	while (!queue.empty())
	{
		auto [d, index] = queue.top();
		queue.pop();
		if (d > path_map.distance[index])
			continue;

		polygon_data* polygon = get_polygon_data(index);
		for (int i = 0; i < polygon->vertex_count; ++i)
		{
			const short adjacent = polygon->adjacent_polygon_indexes[i];
			if (adjacent == NONE || adjacent >= dynamic_world->polygon_count)
				continue;
			line_data* line = get_line_data(polygon->line_indexes[i]);
			if (LINE_IS_SOLID(line) && !LINE_HAS_TRANSPARENT_SIDE(line))
				continue;
			if (line->lowest_adjacent_ceiling - line->highest_adjacent_floor < kMinimumOpening)
				continue;

			const float next = d + distance2d(polygon->center, get_polygon_data(adjacent)->center);
			if (next < path_map.distance[adjacent] && next <= kMaximumPath)
			{
				path_map.distance[adjacent] = next;
				queue.push({next, adjacent});
			}
		}
	}

	return path_map.distance;
}

}

float SoundOcclusion(const world_location3d* source)
{
	if (!Enabled(kGradedOcclusion) || !source)
		return -1.f;

	const world_location3d* listener = _sound_listener_proc();
	if (!listener ||
		source->polygon_index < 0 || source->polygon_index >= dynamic_world->polygon_count ||
		listener->polygon_index < 0 || listener->polygon_index >= dynamic_world->polygon_count)
		return -1.f;

	// Lift the source to mid-height, but keep it inside its room.
	world_point3d from = listener->point;
	world_point3d to = source->point;
	polygon_data* source_polygon = get_polygon_data(source->polygon_index);
	to.z = std::min<world_distance>(to.z + kSourceLift, source_polygon->ceiling_height - kEdgeTolerance);

	const DirectResult direct = walk(listener->polygon_index, from, source->polygon_index, to);
	float occlusion = direct.solid ? 1.f :
		std::min(kMaximumHeightOcclusion, kPerHeightBlock * direct.height_blocks);
	if (occlusion <= 0.f)
		return 0.f;

	// Is there a way round?
	const auto& paths = paths_from(listener->polygon_index);
	const float via_rooms = paths[source->polygon_index];
	if (std::isfinite(via_rooms))
	{
		const world_point2d listener2d = {from.x, from.y}, source2d = {to.x, to.y};
		polygon_data* listener_polygon = get_polygon_data(listener->polygon_index);
		const float path = via_rooms +
			distance2d(listener2d, listener_polygon->center) +
			distance2d(source2d, source_polygon->center);
		const float detour = std::max(0.f, path - distance2d(listener2d, source2d));
		const float round = kRoundCornerMinimum +
			(kRoundCornerMaximum - kRoundCornerMinimum) * std::min(1.f, detour / kFullDetour);
		occlusion = std::min(occlusion, round);
	}

	return occlusion;
}

bool RenderListener(const world_location3d& tick_listener, world_location3d& out)
{
	if (!Enabled(kGradedOcclusion))
		return false;

	out = tick_listener;
	out.velocity = {0, 0, 0};

	// Follow the camera the frame was drawn from, when it is the player's
	// own first-person view.
	if (world_view && world_view->show_weapons_in_hand && current_player_index == local_player_index &&
		get_game_state() == _game_in_progress)
	{
		out.point = world_view->origin;
		out.yaw = world_view->yaw;
		out.pitch = world_view->pitch;
	}
	return true;
}

}
