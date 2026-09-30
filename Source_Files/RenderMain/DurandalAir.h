#ifndef DURANDAL_AIR_H
#define DURANDAL_AIR_H
/*
	DurandalAir.h — Durandal project

	Air that moves (the Rampant tier; docs/PLAN-fifth-tier.md). Dust and
	embers: motes drifting in the rooms in view, made afresh each frame from
	the polygon, their number and the world time (nothing is stored, so a
	film shows the same motes at the same tick). Dust is lit by its room
	and by the dynamic lights near it, so it shows where light falls on it
	(a bolt passing lights up the air it passes through); rooms open to the
	sky carry more of it. Over lava, embers rise off the surface, glowing,
	and fade as they climb. DurandalMetal::DrawMotes draws them at the end
	of the world pass, hidden where a surface is nearer.

	Heat shimmer over lava is in DurandalGL's output pass (the warm part of
	the bloom marks the hot air).

	Read-only on the world: films and saves are unaffected.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "DurandalMetal.h"
#include <vector>

struct view_data;
struct sorted_node_data;

namespace DurandalAir {

// This frame's motes for the polygons in view, lit by `lights`
int Build(const view_data* view, const std::vector<sorted_node_data>& nodes, const DurandalMetal::Light* light_list,
		  int light_count, std::vector<DurandalMetal::Mote>& out);

}

#endif
