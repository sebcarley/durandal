#ifndef DURANDAL_ACOUSTICS_H
#define DURANDAL_ACOUSTICS_H
/*
	DurandalAcoustics.h — Durandal project

	Graded sound occlusion (roadmap A2). Upstream decides "obstructed or
	not" with a top-down line test that ignores height. This returns a
	0–1 occlusion instead:
	  - it walks the same polygon path, but checks the ray's height where
	    it crosses each line, so ledges, steps and window openings count;
	  - when the direct line is blocked by a solid wall, it looks for the
	    shortest route through open doorways and grades the occlusion by
	    the detour, so sound comes round corners.
	Exactly 0 and 1 reproduce upstream's clear and obstructed sounds.

	Sound only: it never touches game state, and uses its own copy of the
	line walk, so the AI's line_is_obstructed() is unchanged.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "world.h"

namespace Durandal {

// Geometric occlusion of a sound at `source` for the current listener,
// 0 (clear) to 1 (fully obstructed); negative when graded occlusion is off,
// in which case callers use upstream's flags.
float SoundOcclusion(const world_location3d* source);

// The OpenAL listener: when graded occlusion is on, follows the rendered
// camera every frame (interpolated position, per-frame look) and has zero
// velocity. Upstream's listener passes unrelated player fields as its
// velocity. Returns false when off (use `tick_listener` unchanged).
bool RenderListener(const world_location3d& tick_listener, world_location3d& out);

}

#endif
