#ifndef DURANDAL_SURFACES_H
#define DURANDAL_SURFACES_H
/*
	DurandalSurfaces.h — Durandal project

	The level's surfaces as a traced ray sees them (the Rampant tier;
	docs/PLAN-fifth-tier.md). The shaders already walk the map to trace rays
	(DurandalLights::BuildMap); this gives each surface a ray can hit what
	it needs to be shaded: per polygon its floor and ceiling, and per edge
	the upper and lower parts of the side there, each with its texture (a
	slice of a colour array made from the 8-bit wall art through its own
	colour ramps, at full brightness), where the texture sits, and its
	current light; landscape surfaces are the sky, drawn from the level's
	landscape by direction. First used by Reflecting Liquids (R2).

	Table layout (DurandalMetal::SetSurfaces): entry 0 the sky's mapping
	(repeats around the circle, azimuth offset, vertical span in radians,
	1 when there is a sky texture); then kPerPolygon float4 per polygon:
	  0 floor, 1 ceiling: (slice, texture origin x, y, light);
	  2 + 2e, 3 + 2e: edge e's upper and lower side parts: (slice, x0,
	  the height the texture hangs from (the part's top + y0), light).
	Slice -1: no texture (grey), -2: the sky.

	Read-only on the world: films and saves are unaffected.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

namespace DurandalSurfaces {

constexpr int kPerPolygon = 18;

// Once per frame, before the first draw: the table for the map as it is
// now (lights, animated textures), and any wall art or sky not yet in the
// colour array. Returns false if it could not be built.
bool Frame();

}

#endif
