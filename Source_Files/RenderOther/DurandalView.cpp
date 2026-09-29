/*
	DurandalView.cpp — Durandal project

	See DurandalView.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "DurandalView.h"

#include "cseries.h"
#include "DurandalPreferences.h"
#include "ChaseCam.h"
#include "computer_interface.h"
#include "interface.h"
#include "lua_script.h"
#include "map.h"
#include "mouse.h"
#include "player.h"
#include "preferences.h"
#include "render.h"

#include <algorithm>
#include <cstdlib>

extern bool game_is_being_replayed();

namespace Durandal {

namespace {

// The following three mirror the lambdas in process_aim_input()
// (GameWorld/physics.cpp); keep them in step if upstream changes them.
fixed_angle clamp_aim(fixed_angle theta, int encoding_bits, bool classic_limits)
{
	const angle encoding_bias = (1 << encoding_bits) / 2;
	const angle encoding_limit = encoding_bias - 1;
	const angle limit = classic_limits ? encoding_bias / 2 : encoding_limit;
	return A1_PIN(theta, -limit * FIXED_ONE, limit * FIXED_ONE);
}

angle round_aim(fixed_angle theta, bool classic_precision)
{
	return classic_precision ?
		SGN(theta) * std::max<angle>(1, std::abs(theta / FIXED_ONE)) :
		(theta + SGN(theta) * FIXED_ONE / 2) / FIXED_ONE;
}

// The change in virtual angle the next tick will make for a requested delta.
fixed_angle predicted_change(fixed_angle requested, fixed_angle residual, int bits, bool classic_precision, bool classic_limits)
{
	const fixed_angle target = clamp_aim(requested + residual, bits, classic_limits);
	const fixed_angle applied = classic_precision ? round_aim(target, true) * FIXED_ONE : target;
	return applied - residual;
}

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

// The virtual-angle change the mouse made in the most recently generated
// tick (see NoteTickLook).
fixed_yaw_pitch last_tick_change = {0, 0};

bool look_is_live()
{
	return Enabled(kPerFrameLook) &&
		get_game_state() == _game_in_progress &&
		get_keyboard_controller_status() &&
		!game_is_being_replayed() &&
		!game_is_networked &&
		input_preferences->input_device == _mouse_yaw_pitch &&
		current_player_index == local_player_index;
}

}

bool PollEveryFrame()
{
	return look_is_live();
}

void NoteTickLook(fixed_yaw_pitch delta, fixed_yaw_pitch residual_before)
{
	const bool classic_precision = !input_preferences->extra_mouse_precision;
	const bool classic_limits = input_preferences->classic_aim_speed_limits;
	last_tick_change.yaw = delta.yaw ? predicted_change(delta.yaw, residual_before.yaw, ABSOLUTE_YAW_BITS, classic_precision, classic_limits) : 0;
	last_tick_change.pitch = delta.pitch ? predicted_change(delta.pitch, residual_before.pitch, ABSOLUTE_PITCH_BITS, classic_precision, classic_limits) : 0;
}

void ApplyPerFrameLook(view_data* view, float interpolated_fraction)
{
	if (!look_is_live() || UseLuaCameras() || ChaseCam_IsActive())
		return;
	if (PLAYER_IS_DEAD(local_player) || PLAYER_IS_TELEPORTING(local_player) ||
		player_in_terminal_mode(local_player_index))
		return;

	const bool classic_precision = !input_preferences->extra_mouse_precision;
	const bool classic_limits = input_preferences->classic_aim_speed_limits;
	const fixed_yaw_pitch pending = peek_mouselook_delta();
	const fixed_yaw_pitch residual = virtual_aim_delta();

	// The interpolated view trails the latest tick by (1 - f) of that
	// tick's change. Give back the mouse's share of that lag (keyboard
	// turning keeps its smooth interpolation), then add what the next tick
	// will make of the mouse movement gathered so far.
	const float lag = interpolated_fraction < 0 ? 0.f : 1.f - interpolated_fraction;

	fixed_angle yaw = view->virtual_yaw + static_cast<fixed_angle>(lag * last_tick_change.yaw);
	if (pending.yaw)
		yaw += predicted_change(pending.yaw, residual.yaw, ABSOLUTE_YAW_BITS, classic_precision, classic_limits);
	yaw = to_unsigned(yaw);

	// Pitch: the same, held within the physics model's limits (as
	// physics_update() does), and left alone while recentring.
	fixed_angle pitch = to_signed(view->virtual_pitch);
	if (!(local_player->variables.flags & _RECENTERING_BIT))
	{
		pitch += static_cast<fixed_angle>(lag * last_tick_change.pitch);
		if (pending.pitch)
			pitch += predicted_change(pending.pitch, residual.pitch, ABSOLUTE_PITCH_BITS, classic_precision, classic_limits);
		_fixed minimum, maximum;
		get_absolute_pitch_range(&minimum, &maximum);
		pitch = A1_PIN(pitch, FIXED_INTEGERAL_PART(minimum) * FIXED_ONE, FIXED_INTEGERAL_PART(maximum) * FIXED_ONE);
	}

	// The coarse angles drive visibility and must agree with the fine ones
	// the rasteriser uses.
	view->virtual_yaw = yaw;
	view->yaw = nearest_angle(yaw);
	view->virtual_pitch = to_unsigned(pitch);
	view->pitch = nearest_angle(pitch);
}

}
