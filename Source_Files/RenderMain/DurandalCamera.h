/*
	DurandalCamera.h — Durandal project

	The camera's own motion for the frame being drawn, computed from
	read-only game state and the mouse (Round 10): the roll of Sidestep
	Sway (C2) and the view pitch of Free Look Beyond Aim (C3). Nothing here
	writes game state: the free look changes only what the tick's aim input
	asks for, which films record as any other input, so films, saves and net
	play replay exactly (a replay shows the aim, not the free look, as the
	extra look is not in the film).

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#ifndef DURANDAL_CAMERA_H
#define DURANDAL_CAMERA_H

#include "world.h"

struct view_data;

namespace DurandalCamera {

// The roll to draw this frame with, in degrees: positive rolls the view
// clockwise (the world's right side rises), as Quake's cl_rollangle when
// strafing right. 0 when Sidestep Sway is off.
float FrameRoll(const view_data* view);

// The frustum's half tangents (right and up) as Rasterizer_Metal::SetView
// builds them from the field of view and the view's shape, without the
// teleport effect's distortion.
void FrustumTangents(const view_data* view, double& xtan, double& ytan);

// Free Look Beyond Aim (C3). Marathon's physics model limits the aim to
// 30 degrees up and down; with True Look the view itself may go further,
// up to where the visibility walk's forward cone ends (90 degrees less
// the vertical half field of view). Called once a tick from vbl.cpp with
// the mouse's look delta before it is encoded into the tick's action
// flags; returns the delta to encode: the mouse's own while the view is
// within the aim limit, otherwise what holds the aim at the limit or
// brings it back under the view.
fixed_yaw_pitch FreeLookTickInput(fixed_yaw_pitch delta);

// After the per-frame look: pitches the view beyond the aim when the free
// look is out there. fraction: what the view was interpolated by, or -1.
void ApplyFreeLook(view_data* view, float interpolated_fraction);

// True while the view looks somewhere other than where it aims, so the
// crosshair must mark the aim: a Lua HUD's own centred reticle is hidden
// (Screen.crosshairs.active reads false) and the engine's crosshair is
// drawn at the aim instead.
bool AimOffCentre();

// Where the aim is on the screen, as an offset in pixels from the centre
// of a view this size (0, 0 when the view looks where it aims).
void CrosshairOffset(int view_width, int view_height, int& dx, int& dy);

}

#endif
