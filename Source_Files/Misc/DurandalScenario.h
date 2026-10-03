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

}

#endif
