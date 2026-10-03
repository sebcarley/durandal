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

}
