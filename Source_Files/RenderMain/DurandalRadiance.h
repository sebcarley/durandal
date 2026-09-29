#ifndef DURANDAL_RADIANCE_H
#define DURANDAL_RADIANCE_H
/*
	DurandalRadiance.h — Durandal project

	Light redistribution (roadmap E4), the CPU side of the surface cache.
	Marathon stores how bright each surface looks; a modern light model
	that replaces that changes every room. This one only moves light around
	within each surface (AUDIT 3.6): every floor, ceiling and wall of the
	map is a patch of lumels (8 per world unit) in one atlas; the GPU traces
	rays from each lumel through the map's polygons and averages what it
	sees, where every surface gives off its own authored brightness times
	its texture's colour (sky through landscape surfaces; reviewed wall
	lights brighter). Nearby hits count for less, so corners, ledges and
	the feet of walls gather less. Each surface is then drawn at its own
	brightness x (its lumel / its patch's average), clamped, so its average
	stays where the level's designers put it, plus a small colour bleed.

	Baked progressively for the patches in view (nearest first, within a
	per-frame budget), then left alone; patches around a moving platform
	are refreshed. Read-only on the world: films and saves are unaffected.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include <vector>

struct view_data;
struct sorted_node_data;
struct side_texture_definition;

namespace DurandalRadiance {

// Once per frame, after SetMap and before the first draw: lays the cache
// out for a new level, notices moving platforms and bakes some of what is
// in view. Returns whether the draws may use patches.
bool Frame(const view_data* view, const std::vector<sorted_node_data>& nodes);

// The patch for a polygon's floor or ceiling, or for the wall drawn with
// `texture` (a side's texture definition); -1 if none
int FloorPatch(short polygon);
int CeilingPatch(short polygon);
int WallPatch(const side_texture_definition* texture);

}

#endif
