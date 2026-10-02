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
#include "media.h"
#include "monsters.h"
#include "player.h"
#include "preferences.h"
#include "shell.h"
#include "Console.h"
#include "Logging.h"

#define DONT_REPEAT_DEFINITIONS
#include "monster_definitions.h"

#include <SDL2/SDL.h>
#include <cstdlib>

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

// Summon BOBs: the key asks, the next tick's start delivers
constexpr short kSummonType = _civilian_security;	// "steve": pistol, toughest, fires every second
constexpr int kSummoned = 5;
bool summon_pending = false;

// Development: DURANDAL_SUMMON_TEST=<tick> summons at that tick of a film
// run (QA gate open), so frame shots can show the cheat; the film goes out
// of step from there, which a look at it does not mind
int summon_test_tick()
{
	static const int tick = [] {
		const char* v = std::getenv("DURANDAL_SUMMON_TEST");
		return v ? std::atoi(v) : -1;
	}();
	return Durandal::QA() ? tick : -1;
}

bool summon_available()
{
	return Durandal::Available() && solo_game();
}

// The polygon a BOB could walk to at `to`, in a straight line from `from`
// (in `polygon_index`): no wall crossed, every polygon on the way within a
// step of `floor` with room for the BOB's height; the one it ends in, when
// `stand` is set, also not a platform, teleporter, exit or hurting floor,
// and no lava, goo or deep liquid over its floor. NONE if not
short walk_to(world_point2d from, short polygon_index, world_point2d to, world_distance floor,
			  world_distance height, bool stand)
{
	for (int crossings = 0; crossings < 32 && polygon_index != NONE; ++crossings)
	{
		const short line_index = find_line_crossed_leaving_polygon(polygon_index, &from, &to);
		if (line_index == NONE)
		{
			if (!stand)
				return polygon_index;
			const polygon_data* polygon = get_polygon_data(polygon_index);
			switch (polygon->type)
			{
				case _polygon_is_platform:
				case _polygon_is_teleporter:
				case _polygon_is_monster_impassable:
				case _polygon_is_automatic_exit:
				case _polygon_is_minor_ouch:
				case _polygon_is_major_ouch:
					return NONE;
			}
			// Shallow water or sewage is fine (M2 has plenty underfoot);
			// lava, goo and anything deeper than half a BOB are not
			if (polygon->media_index != NONE)
				if (const media_data* media = get_media_data(polygon->media_index))
					if (media->height > polygon->floor_height &&
						(media->type == _media_lava || media->type == _media_goo ||
						 media->height - polygon->floor_height > height / 2))
						return NONE;
			return polygon_index;
		}
		if (LINE_IS_SOLID(get_line_data(line_index)))
			return NONE;
		polygon_index = find_adjacent_polygon(polygon_index, line_index);
		if (polygon_index == NONE)
			return NONE;
		const polygon_data* polygon = get_polygon_data(polygon_index);
		if (POLYGON_IS_DETACHED(polygon) || std::abs(polygon->floor_height - floor) > WORLD_ONE / 3 ||
			polygon->ceiling_height - polygon->floor_height < height + WORLD_ONE / 8)
			return NONE;
	}
	return NONE;
}

// Nothing solid (a monster, the player, scenery) within `gap` of `at`
// at the BOB's height
bool clear_of_objects(world_point3d at, world_distance gap, world_distance height)
{
	for (size_t i = 0; i < ObjectList.size(); ++i)
	{
		const object_data* object = &objects[i];
		if (!SLOT_IS_USED(object) || !OBJECT_IS_SOLID(object))
			continue;
		const int64_t dx = object->location.x - at.x, dy = object->location.y - at.y;
		if (dx * dx + dy * dy < int64_t(gap) * gap && std::abs(object->location.z - at.z) < height)
			return false;
	}
	return true;
}

// Five security BOBs beside and behind the local player, nearest first:
// they arrive invisible and are activated, so they teleport in (with the
// sound) and go for the nearest aliens
void summon()
{
	player_data* player = get_player_data(local_player_index);
	if (PLAYER_IS_DEAD(player))
	{
		screen_printf("Summon BOBs: not while dead");
		return;
	}
	const object_data* body = get_object_data(player->object_index);
	const monster_definition* definition = get_monster_definition_external(kSummonType);
	const world_distance radius = definition->radius, height = definition->height;
	const world_point2d centre = { body->location.x, body->location.y };
	const world_distance floor = get_polygon_data(body->polygon)->floor_height;
	// Beside and behind, so they do not stand in the line of fire
	const int degrees[] = { 75, -75, 110, -110, 145, -145, 180, 40, -40 };
	const world_distance rings[] = { WORLD_ONE, 3 * WORLD_ONE / 2, 2 * WORLD_ONE, 3 * WORLD_ONE };
	int placed = 0;
	bool full = false;	// no free monster or object slots
	for (world_distance ring : rings)
		for (int d : degrees)
		{
			if (placed == kSummoned || full)
				break;
			world_point2d p = centre;
			translate_point2d(&p, ring, NORMALIZE_ANGLE(player->facing + d * FULL_CIRCLE / 360));
			const short polygon_index = walk_to(centre, body->polygon, p, floor, height, true);
			if (polygon_index == NONE)
				continue;
			const world_distance spot_floor = get_polygon_data(polygon_index)->floor_height;
			// Room around it: no wall or ledge within the BOB's radius
			bool room = true;
			for (int k = 0; k < 4 && room; ++k)
			{
				world_point2d edge = p;
				translate_point2d(&edge, radius, k * QUARTER_CIRCLE);
				room = walk_to(p, polygon_index, edge, spot_floor, height, false) != NONE;
			}
			if (!room || !clear_of_objects({ p.x, p.y, spot_floor }, 2 * radius + WORLD_ONE / 8, height))
				continue;
			object_location location;
			location.p = { p.x, p.y, 0 };
			location.polygon_index = polygon_index;
			location.yaw = player->facing;
			location.pitch = 0;
			location.flags = _map_object_is_invisible;
			const short monster_index = new_monster(&location, kSummonType);
			if (monster_index == NONE)
			{
				full = true;
				continue;
			}
			activate_monster(monster_index);
			++placed;
		}
	if (placed)
		screen_printf(placed == 1 ? "1 BOB summoned" : "%d BOBs summoned", placed);
	else
		screen_printf("Summon BOBs: no room here");
	logNote("Durandal: summoned %d BOBs at tick %d", placed, int(dynamic_world->tick_count));
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

void MarkCollections()
{
	summon_pending = false;
	// A level restored from a save may hold BOBs summoned earlier: their
	// shapes load whatever the gate says, or they would stand frozen (and a
	// film begun there would replay differently from the game)
	bool present = false;
	for (short i = 0; i < MAXIMUM_MONSTERS_PER_MAP && !present; ++i)
	{
		const monster_data* monster = &monsters[i];
		present = SLOT_IS_USED(monster) && monster->type == kSummonType;
	}
	if (present || (summon_available() && (!game_is_being_replayed() || summon_test_tick() >= 0)))
		mark_monster_collections(kSummonType, true);
}

void BeforeTick()
{
	const int test = summon_test_tick();
	if (test >= 0 && game_is_being_replayed() && dynamic_world->tick_count == test)
		summon_pending = true;
	if (!summon_pending)
		return;
	summon_pending = false;
	if (summon_available() && get_game_state() == _game_in_progress && (!game_is_being_replayed() || test >= 0))
		summon();
}

// The cheat keys: a press (not a hold) switches the cheat in the
// preferences, and the flags follow below. Only while the keyboard is
// the game's (no console or chat typing)
static void check_keys()
{
	static bool god_was_down = false, noclip_was_down = false, summon_was_down = false;
	if (!get_keyboard_controller_status() || Console::instance()->input_active())
	{
		god_was_down = noclip_was_down = summon_was_down = false;
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
	// Summon BOBs: the BOBs arrive before the next tick, and the film
	// stops now, as for a cheat switched in mid-game
	if (pressed(prefs.cheat_summon_key, summon_was_down) && !summon_pending)
	{
		stop_film("summon BOBs");
		summon_pending = true;
	}
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
