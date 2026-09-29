#ifndef DURANDAL_GLOW_H
#define DURANDAL_GLOW_H
/*
	DurandalGlow.h — Durandal project

	Which texels of Marathon 2's wall textures emit light (roadmap E1c).
	M2's walls have no self-luminous colours, so screens, lamps, lit
	switches and lava are picked out by hand: every wall texture of the
	water, lava, sewage and Pfhor sets was reviewed, and those with lights
	are listed with a rule for which of their colours glow. Each entry
	carries a fingerprint of the bitmap's pixels, so the list only applies
	to Marathon 2's own textures (other scenarios' walls never glow by
	mistake).

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include <cstddef>
#include <cstdint>

struct bitmap_definition;

namespace DurandalGlow {

enum Mode : int {
	kNone = 0,
	kColours = 1,	// bright saturated colours: screens, LEDs, lit switches, lava
	kWhite = 2,		// bright whites: light panels and fixtures
	kHot = 4,		// only the very brightest saturated colours
	kRedsOnly = 8	// with kColours: red hues only (red LEDs on yellow panels)
};

// Fingerprint of a bitmap's pixels (as loaded)
uint32_t Fingerprint(const bitmap_definition* bitmap);

// The rule for this wall bitmap; kNone if it has no lights or is not
// Marathon 2's.
int ModeFor(short collection, short bitmap, uint32_t fingerprint);

// 0-255: how much a texel of this full-brightness colour glows.
uint8_t Mask(int mode, const uint8_t* rgb);

}

#endif
