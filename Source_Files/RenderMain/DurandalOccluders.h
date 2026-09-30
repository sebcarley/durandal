#ifndef DURANDAL_OCCLUDERS_H
#define DURANDAL_OCCLUDERS_H
/*
	DurandalOccluders.h — Durandal project

	Traced shadows (R1, the Rampant tier; docs/PLAN-fifth-tier.md). Light
	Shadows walk the map from each lit point to each dynamic light, which
	is exact for walls, steps and ceilings but blind to everything standing
	in the rooms. Here the figures near this frame's lights (monsters and
	players, items, scenery, corpses; in view or not) become cards the walk
	meets as it crosses their polygons: each card turns to face the ray and
	carries its frame's silhouette, rasterised from the 8-bit Marathon
	bitmap into one slice of a mask array with mip levels. Sampling a
	coarser level where the figure stands further from the lit point
	softens the shadow's edge by the light's size, so a shadow is sharp at
	a figure's feet and soft further off, from one ray.

	Read-only on the world: object positions and shapes are read during
	rendering, nothing is written, so films and saves are unaffected.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "DurandalMetal.h"
#include <vector>

struct view_data;

namespace DurandalOccluders {

// Gathers the figures within reach of `light_list` (and, with
// around_viewer, those within a few world units of the viewer, for traced
// ambient shadows), places their silhouettes in the mask array, and hands
// them to the renderer (DurandalMetal::SetOccluders). Returns how many; 0
// clears them.
int Gather(const view_data* view, const DurandalMetal::Light* light_list, int light_count, bool around_viewer = false);

}

#endif
