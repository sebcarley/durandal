/*
	DurandalGlow.cpp — Durandal project

	See DurandalGlow.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "cseries.h"
#include "DurandalGlow.h"
#include "textures.h"
#include <cstdio>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace DurandalGlow {

namespace {

struct Entry {
	short collection;
	short bitmap;
	int mode;
	uint32_t fingerprint;	// of the bitmap's pixels (Fingerprint()), as shipped
};

const int C = kColours, W = kWhite, H = kHot, R = kRedsOnly;

// Reviewed by eye from the shipped Shapes file (bitmaps as numbered there).
const Entry kEntries[] = {
	// Water (collection 17)
	{17, 0, C | R, 0x54248fdb}, {17, 1, C | R, 0x1b73e7b7},	// panels: red LEDs, not the yellow casing
	{17, 2, C, 0x64db97e1}, {17, 3, C, 0x2b2bf6b8},			// ring switches (lit, unlit)
	{17, 4, C, 0x6ab04a93},							// terminal screens
	{17, 9, C, 0x210752fd}, {17, 10, C, 0xe7788bb7}, {17, 15, C, 0x1141720b},	// green light strips and ring
	{17, 16, W | C, 0x642e733e},						// light panels with a red eye
	{17, 25, W, 0x3b131089},							// ceiling light panels
	{17, 29, C, 0xadc90caa},							// red readouts
	// Lava (18)
	{18, 0, C, 0x16e7ce1b}, {18, 1, C, 0x1b8b0232}, {18, 2, C, 0x2a9001e6}, {18, 3, C, 0x041507cc}, {18, 4, C, 0xf7d6ad96},	// panels, switches, screens
	{18, 9, W, 0x43399fd8}, {18, 10, W, 0x069bca94},			// light fixtures
	{18, 11, C, 0x316b2f9d},							// green LEDs
	{18, 12, C, 0xa0452aa2},							// lava
	{18, 13, C, 0x46302ba7}, {18, 21, C, 0xab1bbc70}, {18, 22, C, 0x0ea25589}, {18, 26, C, 0x0ba032a1}, {18, 27, C, 0x71868003},	// readouts, red windows
	{18, 25, W, 0x46e182de},							// strip light
	{18, 29, W, 0xf45f8fd6},							// lit panel behind a grate
	// Sewage (19)
	{19, 0, C, 0xe2b7ff23}, {19, 1, C, 0x068c5969}, {19, 2, C, 0xc75a7500}, {19, 3, C, 0xf2e30f39}, {19, 4, C, 0x86f694bf},	// panels, switches, screens
	{19, 9, W, 0xff9b982b}, {19, 20, W, 0xe59e7df4}, {19, 21, W, 0x88f94ce7}, {19, 22, W, 0xf361c776},	// light panels and windows
	// Pfhor (21): the palette is saturated everywhere, so only real lights
	{21, 0, H, 0xd2acba6f}, {21, 1, H, 0xac40066d},			// switch lights
	{21, 2, C, 0xea73fd70}, {21, 3, C, 0x6db54f44}, {21, 4, C, 0xd67121d6},	// ring switches, screens
	{21, 11, W, 0x7b29844a}, {21, 12, W, 0x5a948f33},			// light pods and cones
	{21, 19, H, 0x757ab5f0}, {21, 23, H, 0xe5276498},			// green light discs
};

const Entry* find(short collection, short bitmap)
{
	for (const Entry& e : kEntries)
		if (e.collection == collection && e.bitmap == bitmap)
			return &e;
	return nullptr;
}

}

uint32_t Fingerprint(const bitmap_definition* bitmap)
{
	if (!bitmap || bitmap->bytes_per_row == NONE)
		return 0;
	const bool columns = (bitmap->flags & _COLUMN_ORDER_BIT) != 0;
	const int lines = columns ? bitmap->width : bitmap->height;
	const int length = columns ? bitmap->height : bitmap->width;
	uint32_t h = 2166136261u;	// FNV-1a
	for (int l = 0; l < lines; ++l)
		for (int i = 0; i < length; ++i)
			h = (h ^ bitmap->row_addresses[l][i]) * 16777619u;
	return h;
}

int ModeFor(short collection, short bitmap, uint32_t fingerprint)
{
	const Entry* e = find(collection, bitmap);
	return (e && e->fingerprint == fingerprint) ? e->mode : kNone;
}

uint8_t Mask(int mode, const uint8_t* rgb)
{
	if (!mode)
		return 0;
	const float r = rgb[0] / 255.f, g = rgb[1] / 255.f, b = rgb[2] / 255.f;
	const float hi = std::max({r, g, b}), lo = std::min({r, g, b});
	const float saturation = hi > 0 ? (hi - lo) / hi : 0;
	auto ramp = [](float x, float a, float c) { return std::clamp((x - a) / (c - a), 0.f, 1.f); };
	float m = 0;
	if (mode & kColours)
	{
		float c = ramp(saturation, 0.55f, 0.8f) * ramp(hi, 0.6f, 0.9f);
		if (mode & kRedsOnly)
		{
			// Hue within about 25 degrees of red
			const bool red = r == hi && (g - b) / std::max(1e-3f, hi - lo) < 0.45f && (b - g) / std::max(1e-3f, hi - lo) < 0.45f;
			if (!red)
				c = 0;
		}
		m = std::max(m, c);
	}
	if (mode & kHot)
		m = std::max(m, ramp(saturation, 0.6f, 0.8f) * ramp(hi, 0.9f, 1.0f));
	if (mode & kWhite)
		m = std::max(m, ramp(hi, 0.8f, 0.95f) * (1 - ramp(saturation, 0.3f, 0.5f)));
	return uint8_t(std::lround(m * 255));
}

}
