#ifndef DURANDAL_LIGHTS_H
#define DURANDAL_LIGHTS_H
/*
	DurandalLights.h — Durandal project

	Dynamic lights (roadmap E2). Each frame, the objects in the visible
	polygons that are drawn with a minimum light (projectiles, explosions and
	other effects, and monsters on their firing frames), plus the viewer's
	own muzzle flash, become point lights that brighten the world around
	them. In the original they only lit themselves (and the flash only the
	shooter's view); this is new behaviour, behind the Dynamic Lights switch.

	Read-only: it looks at object positions and shapes during rendering and
	changes nothing in the world, so films and saves are unaffected.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "DurandalMetal.h"
#include <vector>

struct view_data;
struct sorted_node_data;

namespace DurandalLights {

// Fills `out` (up to DurandalMetal::kMaximumLights, nearest the viewer
// first) and returns the count. weapon_flare: the viewer's muzzle flash,
// 0-1.
int Gather(const view_data* view, const std::vector<sorted_node_data>& nodes, float weapon_flare,
		   DurandalMetal::Light* out);

// Contact shadows: items, monsters (and players) and scenery standing in
// the visible polygons, as soft shade on the floor under them. Fills `out`
// (up to DurandalMetal::kMaximumCasters, nearest the viewer first) and
// returns the count. The viewer's own body casts none.
int GatherCasters(const view_data* view, const std::vector<sorted_node_data>& nodes,
				  DurandalMetal::Caster* out);

// Light shadows (E3): the map as the shaders walk it (9 float4 per polygon,
// current floor and ceiling heights); returns the polygon count.
int BuildMap(std::vector<simd_float4>& out);

}

#endif
