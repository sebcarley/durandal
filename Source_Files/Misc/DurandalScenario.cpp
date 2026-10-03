/*
	DurandalScenario.cpp — Durandal project

	See DurandalScenario.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "DurandalScenario.h"

#include "Scenario.h"

#include <string>

namespace DurandalScenario {

Game Current()
{
	// The scenario is declared by MML read at start-up and never changes
	// after; until it is known, the answer is not cached.
	static bool known = false;
	static Game game = kMarathon2;
	if (known)
		return game;
	const std::string id = Scenario::instance()->GetID();
	if (id.empty())
		return kMarathon2;
	known = true;
	if (id.compare(0, 17, "Marathon Infinity") == 0)
		game = kInfinity;
	else if (id.compare(0, 10, "Marathon 1") == 0 || id == "Marathon")
		game = kMarathon1;
	else
		game = kMarathon2;
	return game;
}

const char* Name(Game game)
{
	switch (game)
	{
		case kInfinity: return "Marathon Infinity";
		case kMarathon1: return "Marathon";
		default: return "Marathon 2";
	}
}

Holds CollectionHolds(int collection)
{
	switch (collection)
	{
		case 1: case 4: case 7:
			return kWeaponsItems;
	}
	if (Current() == kMarathon1)
	{
		// From the packs' MML and the collections' types as loaded (3 Oct
		// 2026): Marathon has no scenery-type collection; 23 and 25 hold its
		// scenery as objects
		switch (collection)
		{
			case 2: case 8: case 17: case 18: case 19: case 24:
				return kWallSet;
			case 3: case 5: case 6: case 9: case 12: case 14: case 15: case 16:
			case 21: case 22: case 26: case 27: case 29:
				return kMonster;
			case 23: case 25:
				return kScenery;
			default:
				return kOther;
		}
	}
	switch (collection)
	{
		case 2: case 3: case 5: case 6: case 8: case 9: case 10: case 11:
		case 12: case 13: case 14: case 15: case 16: case 31:
			return kMonster;
		case 17: case 18: case 19: case 20: case 21:
		case 27: case 28: case 29: case 30:
			return kWallSet;
		case 22: case 23: case 24: case 25: case 26:
			return kScenery;
		default:
			return kOther;
	}
}

}
