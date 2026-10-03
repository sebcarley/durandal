#ifndef DURANDAL_SCENARIO_H
#define DURANDAL_SCENARIO_H
/*
	DurandalScenario.h — Durandal project

	Which of the trilogy is being played (trilogy plan, T1). Durandal was
	built on Marathon 2, and a few of its tables are keyed to that game's
	collection numbers, environment codes and art (the plan's inventory):
	they ask here instead of assuming. The answer comes from the scenario
	the bundled data declares (Scripts/<game>.mml: "Marathon 2", "Marathon
	Infinity", "Marathon 1.2"); a scenario Durandal does not know is
	treated as Marathon 2, as before.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

namespace DurandalScenario {

enum Game {
	kMarathon2 = 0,
	kInfinity,
	kMarathon1,
};

Game Current();

inline bool Marathon1() { return Current() == kMarathon1; }
inline bool Infinity() { return Current() == kInfinity; }

const char* Name(Game game);

// What a shapes collection holds in the game being played. The games
// number them differently: Marathon's walls are 2, 8, 17-19 and 24, its
// Juggernaut 21, Wasp 27 and Cyborg 29, where Marathon 2 and Infinity
// have walls 17-21, landscapes 27-30 and scenery 22-26.
enum Holds {
	kOther = -1,		// interface, unused, or not known
	kWallSet = 0,		// a wall set or landscape
	kMonster,		// a monster or the player
	kWeaponsItems,		// weapons in hand (1), projectiles and explosions (4), items (7)
	kScenery,
};
Holds CollectionHolds(int collection);

}

#endif
