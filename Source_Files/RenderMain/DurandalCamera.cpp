/*
	DurandalCamera.cpp — Durandal project

	See DurandalCamera.h.

	Sidestep Sway (C2) is Quake's view roll (Com_CalcRoll in its common.c,
	cvars cl_rollangle 2 degrees and cl_rollspeed 200 in view.c: side =
	velocity . right; the roll grows with |side| until it reaches the full
	angle at cl_rollspeed) reimplemented on Marathon's physics variables:
	the player's velocity perpendicular to its facing, read only, as a
	fraction of the running model's top sideways speed.

	Free Look Beyond Aim (C3): the mouse's pitch is integrated here into a
	free view pitch. While it is within the physics model's aim limit the
	game is the master (the free pitch is resynced to the aim every tick
	and the mouse delta goes to the game untouched, exactly as before).
	Once it passes the limit the game is asked, every tick, for one unit
	beyond the limit (which its physics clamps, as it does a player pushing
	against the limit, and which keeps it in absolute pitch mode so it
	never auto-recentres meanwhile); when the free pitch comes back inside,
	the game is asked for the exact delta that puts the aim under the view
	again. Shots go where the aim is: the crosshair marks it.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "cseries.h"
#include "DurandalCamera.h"
#include "DurandalPreferences.h"
#include "DurandalMetal.h"
#include "DurandalView.h"

#include "map.h"
#include "player.h"
#include "physics_models.h"
#include "render.h"
#include "interface.h"
#include "ChaseCam.h"
#include "computer_interface.h"
#include "lua_script.h"
#include "mouse.h"
#include "preferences.h"
#include "ViewControl.h"
#include "Crosshairs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// physics.cpp (not in any header): the constants of the level's physics
// model for the given action flags (walking, or running with _run_dont_walk)
struct physics_constants *get_physics_constants_for_model(short physics_model, uint32 action_flags);
extern bool game_is_being_replayed();
extern struct view_data *world_view;	// screen.cpp (screen_shared.h)

namespace {

// Quake: cl_rollangle 2 at cl_rollspeed 200, with strafe speeds of 320
// running and 160 walking, so running gives the full angle and walking
// 80% of it. Marathon walks sideways at 13/20 of its running speed, so the
// full angle is set at 80% of the running speed: running 2 degrees,
// walking 1.6. Tuned in play.
const float kRollAngle = 2.0f;			// degrees at full sway
const float kFullRollAtSpeed = 0.8f;	// fraction of the running model's top sideways speed

// The physics steps sideways speed once a tick (30 Hz); the roll glides
// towards its target with this time constant, in seconds, so the steps
// never show at high frame rates
const float kGlideSeconds = 0.08f;

float smoothed_roll = 0;
double last_time = -1;	// world time of the previous frame, in ticks

// DURANDAL_CAMERA_LOG=1 prints the sideways speed and roll once a tick,
// and the free look's state as it changes
bool Logging()
{
	static const bool wanted = std::getenv("DURANDAL_CAMERA_LOG") != nullptr;
	return wanted;
}

// Free look state. Angles are signed fixed angles (FIXED_ONE per Marathon
// angle unit, 512 units to the circle), as the physics keeps the aim.
fixed_angle free_previous = 0;	// the free view pitch after the previous tick
fixed_angle free_latest = 0;	// and after the latest one
bool beyond = false;			// the latest tick's view is beyond the aim limit
// This frame: the aim the game shows, the view drawn, and whether they differ
fixed_angle frame_aim = 0;
fixed_angle frame_view = 0;
bool frame_free = false;

const double kAnglesPerDegree = double(NUMBER_OF_ANGLES) / 360.0;

fixed_angle to_signed(fixed_angle a)
{
	const fixed_angle full = FULL_CIRCLE * FIXED_ONE;
	a %= full;
	if (a < 0) a += full;
	return a >= HALF_CIRCLE * FIXED_ONE ? a - full : a;
}

fixed_angle to_unsigned(fixed_angle a)
{
	const fixed_angle full = FULL_CIRCLE * FIXED_ONE;
	a %= full;
	return a < 0 ? a + full : a;
}

angle nearest_angle(fixed_angle a)
{
	return NORMALIZE_ANGLE(static_cast<angle>((a + FIXED_ONE / 2) >> FIXED_FRACTIONAL_BITS));
}

// The aim the game shows the local player: the physical elevation to
// whole units plus the virtual residual, as physics_update() clamps it
fixed_angle current_aim()
{
	return FIXED_INTEGERAL_PART(local_player->variables.elevation) * FIXED_ONE + virtual_aim_delta().pitch;
}

// The aim limits as physics_update() applies them to the virtual pitch
void aim_limits(fixed_angle& low, fixed_angle& high)
{
	_fixed minimum, maximum;
	get_absolute_pitch_range(&minimum, &maximum);
	low = FIXED_INTEGERAL_PART(minimum) * FIXED_ONE;
	high = FIXED_INTEGERAL_PART(maximum) * FIXED_ONE;
}

// How far the view may pitch: the visibility walk covers the half plane
// in front of the viewer, so no ray of the frustum may point behind the
// vertical. With the sway's roll allowed for, the top corner's up tangent
// is tan v cos r + tan h sin r.
fixed_angle free_look_limit(const view_data* view)
{
	double xtan = 0.75, ytan = 0.42;
	if (view)
		DurandalCamera::FrustumTangents(view, xtan, ytan);
	const double roll = 2.5 * M_PI / 180.0;
	const double up = ytan * std::cos(roll) + xtan * std::sin(roll);
	const double degrees = std::clamp(89.0 - std::atan(up) * 180.0 / M_PI, 10.0, 89.0);
	return fixed_angle(degrees * kAnglesPerDegree * FIXED_ONE);
}

// The free look acts on the local player's own live mouse look: never a
// replay or a net game, and only where the per-frame look also applies
bool free_look_live()
{
	return Durandal::Enabled(Durandal::kFreeLook) && Durandal::Enabled(Durandal::kTrueLook) &&
		DurandalMetal::WorldRendererActive() &&
		get_game_state() == _game_in_progress &&
		get_keyboard_controller_status() &&
		!game_is_being_replayed() &&
		!game_is_networked &&
		input_preferences->input_device == _mouse_yaw_pitch &&
		current_player_index == local_player_index &&
		!UseLuaCameras() && !ChaseCam_IsActive();
}

bool player_can_look()
{
	return !PLAYER_IS_DEAD(local_player) && !PLAYER_IS_TELEPORTING(local_player) &&
		!player_in_terminal_mode(local_player_index) &&
		!(local_player->variables.flags & _RECENTERING_BIT);
}

}

float DurandalCamera::FrameRoll(const view_data* view)
{
	if (!view || !current_player || !Durandal::Enabled(Durandal::kSidestepSway))
	{
		smoothed_roll = 0;
		last_time = -1;
		return 0;
	}

	// The sideways speed as a fraction of the running model's top: positive
	// is to the right (physics.cpp moves along facing + 90 degrees for a
	// positive perpendicular_velocity, which the view sees as its +y, the
	// right-hand side of the screen)
	float target = 0;
	const physics_constants* running = get_physics_constants_for_model(static_world->physics_model, _run_dont_walk);
	if (running && running->maximum_perpendicular_velocity > 0)
	{
		float side = current_player->variables.perpendicular_velocity / float(running->maximum_perpendicular_velocity);
		const float sign = side < 0 ? -1.0f : 1.0f;
		side = std::abs(side);
		target = sign * (side < kFullRollAtSpeed ? side / kFullRollAtSpeed * kRollAngle : kRollAngle);
	}

	// Glide on world time (ticks plus the fraction of the next), so a film
	// shows the same roll every time it is played
	const double now = double(view->tick_count) + view->heartbeat_fraction;
	const double dt = last_time < 0 ? 0 : (now - last_time) / TICKS_PER_SECOND;
	last_time = now;
	if (dt < 0 || dt > 1)
	{
		// A jump in time (new level, a film started): no glide
		smoothed_roll = target;
		return smoothed_roll;
	}
	const float alpha = 1.0f - std::exp(-float(dt) / kGlideSeconds);
	smoothed_roll += (target - smoothed_roll) * alpha;
	if (target == 0 && std::abs(smoothed_roll) < 0.001f)
		smoothed_roll = 0;	// settled: no perpetual sliver of roll (or log line)
	if (Logging())
	{
		static uint32 logged_tick = ~0u;
		if (view->tick_count != logged_tick && (target != 0 || smoothed_roll != 0))
		{
			logged_tick = view->tick_count;
			std::fprintf(stderr, "Durandal camera: tick %u sideways %+d target %+.2f roll %+.2f\n",
				unsigned(view->tick_count), int(current_player->variables.perpendicular_velocity), target, smoothed_roll);
		}
	}
	return smoothed_roll;
}

void DurandalCamera::FrustumTangents(const view_data* view, double& xtan, double& ytan)
{
	const double deg2rad = M_PI / 180.0;
	const double aspect = view->screen_height > 0 ? view->screen_width / double(view->screen_height) : 2.0;
	if (View_FOV_FixHorizontalNotVertical())
	{
		xtan = std::tan(view->field_of_view * deg2rad / 2.0);
		ytan = xtan / aspect;
	}
	else
	{
		ytan = std::tan(view->field_of_view * deg2rad / 2.0) / 2.0;
		xtan = ytan * aspect;
	}
}

fixed_yaw_pitch DurandalCamera::FreeLookTickInput(fixed_yaw_pitch delta)
{
	if (!free_look_live())
	{
		beyond = false;
		return delta;
	}

	const fixed_angle aim = current_aim();
	fixed_angle low, high;
	aim_limits(low, high);
	const fixed_angle limit = free_look_limit(world_view);

	free_previous = free_latest;
	if (!beyond)
		free_latest = aim;	// within the limit the game is the master
	free_latest = std::clamp(free_latest + delta.pitch, -limit, limit);

	if (!player_can_look())
	{
		// Dead, teleporting, reading or recentring: the view follows the aim
		free_latest = aim;
		beyond = false;
		return delta;
	}

	const bool was_beyond = beyond;
	if (free_latest > high)
	{
		beyond = true;
		delta.pitch = (high + FIXED_ONE) - aim;
	}
	else if (free_latest < low)
	{
		beyond = true;
		delta.pitch = (low - FIXED_ONE) - aim;
	}
	else
	{
		beyond = false;
		if (was_beyond)
			delta.pitch = free_latest - aim;	// bring the aim back under the view
	}
	if (Logging() && (beyond || was_beyond))
		std::fprintf(stderr, "Durandal camera: tick %u free look %+.1f aim %+.1f asks %+.2f (limit %.1f)\n",
			unsigned(dynamic_world->tick_count), free_latest / (kAnglesPerDegree * FIXED_ONE),
			aim / (kAnglesPerDegree * FIXED_ONE), delta.pitch / (kAnglesPerDegree * FIXED_ONE),
			limit / (kAnglesPerDegree * FIXED_ONE));
	return delta;
}

void DurandalCamera::ApplyFreeLook(view_data* view, float interpolated_fraction)
{
	frame_aim = frame_view = to_signed(view->virtual_pitch);
	frame_free = false;

	// Development (film runs have no live mouse): DURANDAL_LOOK_OFFSET=<degrees>
	// pitches the view that far beyond the aim, so the free look's rendering
	// and crosshair can be checked from a film
	static const double dev_offset = std::getenv("DURANDAL_LOOK_OFFSET") ? std::atof(std::getenv("DURANDAL_LOOK_OFFSET")) : 0.0;
	if (dev_offset != 0.0 && Durandal::Enabled(Durandal::kTrueLook) && get_game_state() == _game_in_progress)
	{
		const fixed_angle limit = free_look_limit(view);
		const fixed_angle pitched = std::clamp(frame_aim + fixed_angle(dev_offset * kAnglesPerDegree * FIXED_ONE), -limit, limit);
		view->virtual_pitch = to_unsigned(pitched);
		view->pitch = nearest_angle(pitched);
		frame_view = pitched;
		frame_free = true;
		return;
	}

	if (!free_look_live() || !player_can_look())
		return;

	fixed_angle low, high;
	aim_limits(low, high);
	const fixed_angle limit = free_look_limit(view);

	// Where the free view is now: with the per-frame look live, the latest
	// tick's plus the mouse movement gathered since (which the next tick
	// will apply); otherwise interpolated between the last two ticks
	fixed_angle free_view;
	if (Durandal::PollEveryFrame())
		free_view = free_latest + peek_mouselook_delta().pitch;
	else if (interpolated_fraction >= 0)
		free_view = free_previous + fixed_angle((free_latest - free_previous) * interpolated_fraction);
	else
		free_view = free_latest;
	free_view = std::clamp(free_view, -limit, limit);

	if (!beyond && free_view <= high && free_view >= low)
		return;	// within the limit: the aim is the view, as before

	view->virtual_pitch = to_unsigned(free_view);
	view->pitch = nearest_angle(free_view);
	frame_view = free_view;
	frame_free = true;
}

bool DurandalCamera::AimOffCentre()
{
	return frame_free && frame_aim != frame_view;
}

void DurandalCamera::CrosshairOffset(int view_width, int view_height, int& dx, int& dy)
{
	dx = dy = 0;
	if (!frame_free || !world_view || view_height <= 0)
		return;

	// The aim's direction relative to the view's, on the screen: the view
	// pitches (and rolls) about the eye, the aim stays where the physics
	// has it
	double xtan, ytan;
	FrustumTangents(world_view, xtan, ytan);
	const double delta = (frame_aim - frame_view) / (kAnglesPerDegree * FIXED_ONE) * M_PI / 180.0;
	const double half_height = view_height / 2.0;
	double y;
	if (std::abs(delta) >= 89.0 * M_PI / 180.0)
		y = delta < 0 ? 1e6 : -1e6;	// behind the view's vertical: pin to the edge
	else
		y = -std::tan(delta) / ytan * half_height;	// screen y runs downwards
	const double roll = world_view->durandal_roll * M_PI / 180.0;
	double x = y * std::sin(roll);
	y = y * std::cos(roll);

	// Keep the whole marker on the view, and a little in from the edge
	// where HUD panels and the weapon in hand sit (the crosshair renderers
	// scale it with the view as here)
	const CrosshairData& crosshairs = GetCrosshairData();
	const int scale = std::max(1, view_height / 360);
	const double margin = (crosshairs.FromCenter + crosshairs.Length + crosshairs.Thickness) * scale + 2 + view_height * 0.06;
	x = std::clamp(x, -(view_width / 2.0 - margin), view_width / 2.0 - margin);
	y = std::clamp(y, -(half_height - margin), half_height - margin);
	dx = int(std::lround(x));
	dy = int(std::lround(y));
	if (Logging())
	{
		static uint32 logged_tick = ~0u;
		if (world_view->tick_count != logged_tick)
		{
			logged_tick = world_view->tick_count;
			std::fprintf(stderr, "Durandal camera: tick %u crosshair %+d %+d (view %dx%d, aim %+.1f view %+.1f)\n",
				unsigned(world_view->tick_count), dx, dy, view_width, view_height,
				frame_aim / (kAnglesPerDegree * FIXED_ONE), frame_view / (kAnglesPerDegree * FIXED_ONE));
		}
	}
}
