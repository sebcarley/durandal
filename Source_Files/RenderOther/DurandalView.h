#ifndef DURANDAL_VIEW_H
#define DURANDAL_VIEW_H
/*
	DurandalView.h — Durandal project

	Per-frame mouse look (roadmap F1). The engine reads the mouse once per
	30 Hz tick and then interpolates the view between the last two ticks,
	so look lags by up to two ticks. This shows the view the NEXT tick will
	produce from the mouse movement gathered so far, every frame.

	Keyboard turning keeps its interpolation; only the mouse's share of the
	lag is removed. Single-player only: net games keep upstream behaviour.

	Render-side only: aim still enters the simulation through the tick's
	action flags exactly as before, so films, saves and net games are
	unaffected. The prediction repeats the tick's own clamping and rounding,
	so the view does not jump when the tick lands.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "world.h"

struct view_data;

namespace Durandal {

// Call after interpolate_world_view(), with the fraction it interpolated
// by, or -1 if it left the view at the latest tick. Single-player only.
void ApplyPerFrameLook(view_data* view, float interpolated_fraction);

// Called where the tick's aim is sampled (vbl.cpp), with the mouse delta
// and the virtual-aim residual before process_aim_input(). Records the
// view change that tick will make, for ApplyPerFrameLook.
void NoteTickLook(fixed_yaw_pitch delta, fixed_yaw_pitch residual_before);

// True when the main loop should poll events on every pass, so mouse
// motion reaches the prediction without waiting for the 60 Hz poll.
bool PollEveryFrame();

}

#endif
