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
	// Infinity and Marathon (trilogy, 3 Oct 2026): reviewed by eye from
	// contact sheets of each game's own bitmaps (DURANDAL_DUMP_BITMAPS over
	// every test film's level); lights, screens, switches, lava
	// Infinity water (17)
	{17, 0, C | R, 0x92215822}, {17, 1, C | R, 0x2df9383a}, {17, 2, W | C, 0x3ddda058}, {17, 3, C, 0xe7fc6ff6},
	{17, 4, C, 0xe488074c}, {17, 8, W, 0xfaafa78b}, {17, 9, W, 0x7f335962}, {17, 10, W, 0x11e85630},
	{17, 15, W, 0x396fcc62}, {17, 16, W, 0xebf74f65}, {17, 25, W, 0xea143138}, {17, 27, W, 0x859bbaab},
	{17, 29, C, 0xb8d342c3},
	// Infinity lava (18)
	{18, 0, W | C, 0x737532ee}, {18, 1, C, 0x22f626bd}, {18, 2, W | C, 0x3ddda058}, {18, 3, C, 0xe7fc6ff6},
	{18, 4, C, 0xfed71a1d}, {18, 9, W, 0x9fa8878e}, {18, 10, W, 0x896d66a4}, {18, 12, C, 0x03d04cb9},
	{18, 17, W, 0xb36c94fc}, {18, 21, W, 0xf4345953}, {18, 25, W, 0x8acbdc6e},
	// Infinity sewage (19)
	{19, 0, W | C, 0xafab180d}, {19, 1, C, 0x63ec72c3}, {19, 2, W | C, 0x85e4ea10}, {19, 3, C, 0x4284c203},
	{19, 4, C, 0x0a76cc72}, {19, 9, W, 0x5134f24c}, {19, 20, W, 0xf6601b05}, {19, 22, W, 0x2b811a03},
	{19, 29, W, 0x304f3781},
	// Infinity Jjaro (20), reviewed for the first time
	{20, 0, C, 0xaad4cdf9}, {20, 1, C, 0x00b6c728}, {20, 2, C, 0xf1e5b7a6}, {20, 3, C, 0x033376f0},
	{20, 4, C, 0x146c162a}, {20, 17, W, 0x59d0451b}, {20, 20, W, 0x1179d626}, {20, 22, W, 0x03f77da4},
	{20, 32, W, 0x7135879b},
	// Infinity Pfhor (21)
	{21, 0, H, 0x761f89a0}, {21, 1, H, 0xb5d5ad96}, {21, 2, C, 0xeeab00d6}, {21, 3, C, 0x9d089fc6},
	{21, 4, C, 0x0a8cf3e7}, {21, 12, W, 0xa20245e3}, {21, 15, H, 0x371fb6ad}, {21, 19, H, 0xa1ab6df9},
	{21, 23, H, 0x20f91417}, {21, 24, H, 0x453b7b9e}, {21, 26, H, 0xa140a128},
	// Marathon alien (2)
	{2, 0, W, 0xa8507de6}, {2, 10, W, 0xabcc30e1}, {2, 15, W, 0x9ed8a5fc}, {2, 19, W, 0x1d9e68d9},
	{2, 22, C, 0x46046812}, {2, 23, C, 0xb878019e},
	// Marathon computers (8)
	{8, 0, C, 0x283ee4d7}, {8, 1, C, 0x7f8b4855}, {8, 2, C, 0x75253e8d}, {8, 3, C, 0xc37d471e},
	{8, 4, C, 0x97b762ba}, {8, 5, C, 0xf459ec17}, {8, 6, C, 0xb9b5e92a}, {8, 7, C, 0x5ada47a0},
	// Marathon ship (17)
	{17, 6, W, 0xf133dd2f}, {17, 12, W | C, 0x207828b7}, {17, 21, W, 0x226deeff}, {17, 27, C, 0x535e7e58},
	// Marathon ship (18)
	{18, 6, W, 0x8470f57a}, {18, 8, C, 0x27686796}, {18, 9, C, 0x089dfb2c}, {18, 14, W, 0xa683cc05},
	// Marathon ship (19)
	{19, 4, C, 0x40b53f02}, {19, 9, W, 0xa09cd7fd}, {19, 15, C, 0x691c5a84}, {19, 16, C, 0xfd0aec43},
	{19, 19, C, 0x6bd458eb}, {19, 24, C, 0x01972ad4}, {19, 26, W | C, 0x16541ecf}, {19, 29, C, 0x4f5bef00},
	// Marathon alien panels (24)
	{24, 0, C, 0x8bf41b26}, {24, 1, C, 0x611b127e}, {24, 2, C, 0x0f9c0b92}, {24, 3, C, 0x2d25ba4c},
	{24, 5, C, 0x47b88da3},
};

// The fingerprint picks the game: the three games' wall sets share
// collection numbers (Infinity's 17-21) or reuse them for other things
// (Marathon 1's), but never the same pixels
const Entry* find(short collection, short bitmap, uint32_t fingerprint)
{
	for (const Entry& e : kEntries)
		if (e.collection == collection && e.bitmap == bitmap && e.fingerprint == fingerprint)
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
	const Entry* e = find(collection, bitmap, fingerprint);
	return e ? e->mode : kNone;
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
