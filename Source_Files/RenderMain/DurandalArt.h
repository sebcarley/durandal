#ifndef DURANDAL_ART_H
#define DURANDAL_ART_H
/*
	DurandalArt.h — Durandal project

	HD art (Round 11): which installed plugins carry replacement art, and
	the switches that decide whether the game uses them.

	The art itself is never shipped: Bungie's notice on the Freeverse XBLA
	renders (and on everything the community derived from them) allows
	Marathon-community use only, so the packs stay user-installed plugins
	in ~/Library/Application Support/Durandal/Plugins (docs/HD_ASSETS.md
	catalogues them). Aleph One's own plugin loader, MML parser, shapes
	patches and image decoders do the loading; Durandal only decides which
	packs are enabled, by the art they replace (walls and landscapes,
	monsters, weapons and items, scenery), from the HD Art switches on the
	DURANDAL dialog's ART tab. A pack follows the category most of its
	entries replace (CFP Monsters also replaces a few alien projectiles;
	the all-in-one SuperPlugin is mostly monsters). While the gate is
	open that decision overrides the Environment > Plugins dialog for
	these packs; with the gate closed the plugins behave as in stock
	Aleph One.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include <cstdint>
#include <string>
#include <vector>

#include "DurandalPreferences.h"

namespace DurandalArt {

enum Category {
	kWalls = 0,		// wall sets (collections 17-21) and landscapes (27-30)
	kMonsters,		// 2, 3, 5, 6, 8-16, 31: every monster and the player
	kWeapons,		// weapons in hand (1), projectiles and explosions (4), items (7)
	kScenery,		// 22-26
	kModels,		// <model> entries: 3D pickups (the 3D Items plugin)
	kNumberOfCategories
};

struct Pack {
	std::string name;		// the plugin's name
	std::string version;
	std::string path;		// its directory, as Plugin::directory.GetPath()
	int entries[kNumberOfCategories];	// replacement textures and models it declares, per category
	Category category;		// the one most of them replace: the switch it follows
	bool normal_maps;		// carries offset_image (normal) maps
};

// The replacement-art plugins found (scanned once; the plugin list is
// fixed for the process).
const std::vector<Pack>& Packs();

// Soundtrack plugins: those that give the game music, through a map
// patch or MML naming <music> tracks, or a solo Lua script driving the
// Music API. Only one plays at a time (a Lua script's tracks would stack
// on a map patch's playlist), chosen by name in Durandal::Prefs().
// soundtrack; empty means none.
struct Soundtrack {
	std::string name;
	std::string version;
	std::string path;
	const char* kind;		// "map patch", "Lua" or "MML"
};
const std::vector<Soundtrack>& Soundtracks();

// Enables or disables each pack from the HD Art switches, and each
// soundtrack plugin from the Soundtrack choice (none in Stock). Returns
// true when a plugin's state changed, so the caller reloads the MML.
// Does nothing with the gate closed.
bool Apply();

Durandal::Feature FeatureFor(Category category);
const char* CategoryName(Category category);

// The packs that follow a category's switch: "name version, name
// version", or empty when there are none.
std::string Installed(Category category);

}

#endif
