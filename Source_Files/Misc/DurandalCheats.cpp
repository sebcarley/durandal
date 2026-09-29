/*
	DurandalCheats.cpp — Durandal project

	See DurandalCheats.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "cseries.h"
#include "DurandalCheats.h"
#include "DurandalPreferences.h"

#include "interface.h"
#include "map.h"
#include "player.h"
#include "preferences.h"
#include "shell.h"
#include "Console.h"
#include "Logging.h"

#include <SDL2/SDL.h>

bool game_is_being_replayed();
bool game_is_being_recorded();
void durandal_give_all_weapons(short player_index);	// items.cpp
void mark_shield_display_as_dirty();
void mark_oxygen_display_as_dirty();

namespace DurandalCheats {

namespace {

// A single-player game (being played or replayed)
bool solo_game()
{
	return dynamic_world && dynamic_world->player_count == 1 && !game_is_networked;
}

// The cheats this game runs with (from its game data, not the preferences)
unsigned short game_flags()
{
	return solo_game() ? (dynamic_world->game_information.cheat_flags & kFlagMask) : 0;
}

// A cheat is changing the game from here on: its film could not be replayed
void stop_film(const char* what)
{
	if (game_is_being_recorded())
	{
		logNote("Durandal: %s switched in mid-game; this game's film recording stops here", what);
		stop_recording();
	}
}

}

bool LevelSelect()
{
	return Durandal::Available() && Durandal::Prefs().cheat_level_select;
}

unsigned short NewGameFlags()
{
	if (!Durandal::Available())
		return 0;
	const Durandal::Preferences& prefs = Durandal::Prefs();
	return (prefs.cheat_god ? kGodModeFlag : 0) | (prefs.cheat_all_weapons ? kAllWeaponsFlag : 0) |
		(prefs.cheat_noclip ? kNoclipFlag : 0);
}

bool God(short player_index)
{
	return player_index == local_player_index && (game_flags() & kGodModeFlag);
}

bool Noclip(short player_index)
{
	return player_index == local_player_index && (game_flags() & kNoclipFlag);
}

void PlayerTick(short player_index)
{
	if (!God(player_index))
		return;
	player_data* player = get_player_data(player_index);
	if (PLAYER_IS_DEAD(player))
		return;
	if (player->suit_energy < PLAYER_MAXIMUM_SUIT_ENERGY)
	{
		player->suit_energy = PLAYER_MAXIMUM_SUIT_ENERGY;
		mark_shield_display_as_dirty();
	}
	if (player->suit_oxygen < PLAYER_MAXIMUM_SUIT_OXYGEN)
	{
		player->suit_oxygen = PLAYER_MAXIMUM_SUIT_OXYGEN;
		mark_oxygen_display_as_dirty();
	}
}

void LevelBegins()
{
	if (game_flags() & kAllWeaponsFlag)
		durandal_give_all_weapons(local_player_index);
}

// The cheat keys: a press (not a hold) switches the cheat in the
// preferences, and the flags follow below. Only while the keyboard is
// the game's (no console or chat typing)
static void check_keys()
{
	static bool god_was_down = false, noclip_was_down = false;
	if (!get_keyboard_controller_status() || Console::instance()->input_active())
	{
		god_was_down = noclip_was_down = false;
		return;
	}
	Durandal::Preferences& prefs = Durandal::Prefs();
	const Uint8* keys = SDL_GetKeyboardState(nullptr);
	auto pressed = [keys](int scancode, bool& was_down) {
		const bool down = scancode > SDL_SCANCODE_UNKNOWN && scancode < SDL_NUM_SCANCODES && keys[scancode];
		const bool edge = down && !was_down;
		was_down = down;
		return edge;
	};
	bool changed = false;
	if (pressed(prefs.cheat_god_key, god_was_down))
	{
		prefs.cheat_god = !prefs.cheat_god;
		screen_printf("God mode %s", prefs.cheat_god ? "on" : "off");
		changed = true;
	}
	if (pressed(prefs.cheat_noclip_key, noclip_was_down))
	{
		prefs.cheat_noclip = !prefs.cheat_noclip;
		screen_printf("Noclip %s", prefs.cheat_noclip ? "on" : "off");
		changed = true;
	}
	if (changed)
		write_preferences();
}

void Update()
{
	// Only a game being played changes with the preferences; a replay runs
	// with the cheats its film was recorded with
	if (!Durandal::Available() || get_game_state() != _game_in_progress || !solo_game() || game_is_being_replayed())
		return;
	check_keys();
	const unsigned short wanted = NewGameFlags();
	unsigned short& flags = reinterpret_cast<unsigned short&>(dynamic_world->game_information.cheat_flags);
	const unsigned short have = flags & kFlagMask;
	if (wanted == have)
		return;
	if ((wanted ^ have) & kGodModeFlag)
		stop_film("god mode");
	if ((wanted ^ have) & kAllWeaponsFlag)
		stop_film("all weapons");
	if ((wanted ^ have) & kNoclipFlag)
		stop_film("noclip");
	flags = (flags & ~kFlagMask) | wanted;
	if ((wanted & ~have) & kAllWeaponsFlag)
		LevelBegins();	// switched on mid-level: the weapons now
}

}
