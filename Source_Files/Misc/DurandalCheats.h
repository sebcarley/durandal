#ifndef DURANDAL_CHEATS_H
#define DURANDAL_CHEATS_H
/*
	DurandalCheats.h — Durandal project

	Testing cheats (Preferences > DURANDAL > CHEATS): level select when
	beginning a new game, god mode, every weapon with full ammunition at
	the start of each level, and noclip (walls, monsters and scenery do
	not stop the player; the void does). God mode and noclip also switch
	on and off with a key each during a game (Cheats tab; G and N unless
	changed). Summon BOBs (in QA; C unless changed) brings five armed
	security BOBs in beside and behind the player, teleporting in, to stand
	with them in a fight.

	They only act in single-player games. The cheats a game is begun with
	are recorded in its game data (game_data::cheat_flags, spare bits that
	films and saved games carry), and act from inside the game tick, so a
	film recorded with them replays exactly, and a film recorded without
	them never sees them, whatever the preferences say. Switching a cheat
	on or off in the middle of a game changes the game at that point and
	stops its film recording (the film stays valid up to there); so does
	summoning BOBs, which a film cannot carry, and a replay never summons.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

namespace DurandalCheats {

// Bits in game_data::cheat_flags (upstream uses 0x0001-0x0020 for its
// net-game options): the cheats this game runs with
enum { kNoclipFlag = 0x2000, kGodModeFlag = 0x4000, kAllWeaponsFlag = 0x8000,
	kFlagMask = kNoclipFlag | kGodModeFlag | kAllWeaponsFlag };

// interface.cpp: Begin New Game asks which level to start on
bool LevelSelect();

// interface.cpp, a new single-player game: the flags for the cheats
// switched on in the preferences (0 with the gate closed)
unsigned short NewGameFlags();

// player.cpp (damage_player): absorb all damage to this player
bool God(short player_index);

// physics.cpp (instantiate_physics_variables): this player walks through
// walls, monsters and scenery
bool Noclip(short player_index);

// player.cpp (update_players, each tick): god mode keeps this player's
// energy and oxygen full
void PlayerTick(short player_index);

// marathon2.cpp (entering_map): a level begins; the all-weapons cheat
// gives the local player every weapon and full ammunition
void LevelBegins();

// marathon2.cpp (entering_map, before load_collections): while Summon BOBs
// can be used, the security BOB's shapes load with every level (a level
// with no BOBs of its own would not have them, and a monster's animation
// drives its attacks)
void MarkCollections();

// marathon2.cpp (update_world, before each tick, with the world as it
// stands rather than interpolated): BOBs asked for by the key arrive
void BeforeTick();

// shell.cpp main loop: while a game is being played (not replayed), the
// god mode and noclip keys switch their cheats, the summon key asks for
// BOBs (they arrive before the next tick), and a cheat switched on
// or off (there or in the preferences) takes effect, and the film
// recording stops, as the change could not be replayed
void Update();

}

#endif
