/*
	DurandalShading.cpp — Durandal project

	See DurandalShading.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "cseries.h"
#include "DurandalShading.h"
#include "collection_definition.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace DurandalShading {

void DebugDump(short collection, short clut);

namespace {

std::unordered_map<uint32_t, Ramps> tables;
uint32_t generation = 1;

// Runs at least this long count as authored ramps (M2's are 12-19)
const short kMinimumRamp = 6;

uint32_t key(short collection, short clut)
{
	return (uint32_t(uint16_t(collection)) << 16) | uint16_t(clut);
}

// As shapes.cpp's new_color_run(): a brighter colour starts a new run
bool new_run(const rgb_color_value& next, const rgb_color_value& last)
{
	return int32(last.red) + int32(last.green) + int32(last.blue) <
		int32(next.red) + int32(next.green) + int32(next.blue);
}

}

void BeginColorEnvironment()
{
	tables.clear();
	++generation;
}

void Build(short collection, short clut, const rgb_color_value* colors, short color_count, const uint8_t* remap)
{
	Ramps& r = tables[key(collection, clut)];

	if (!remap)
	{
		// Primary table: the runs, exactly as get_next_color_run() finds them
		std::memset(r.data[0], 0, sizeof(r.data[0]));
		int in_ramps = 0;
		short start = 0, count = 0;
		while (start + count < color_count)
		{
			start += count;
			for (count = 0; start + count < color_count; ++count)
				if (count && new_run(colors[start + count], colors[start + count - 1]))
					break;
			if (count >= kMinimumRamp)
				in_ramps += count;
			for (short i = 0; i < count; ++i)
			{
				uint8_t* info = r.data[0][start + i];
				info[0] = uint8_t(start);
				info[1] = uint8_t(i);
				info[2] = uint8_t(std::min<short>(count, 255));
				info[3] = uint8_t((start ? 1 : 0) | ((colors[start + i].flags & SELF_LUMINESCENT_COLOR_FLAG) ? 0x80 : 0));
			}
		}
		// Only colour tables laid out as ramps can be shaded by walking
		// them. M2's weapons in hand (and landscapes) have unordered
		// palettes, where the walk would jump between unrelated colours;
		// they keep true-colour shading (Get() returns null).
		if (in_ramps * 2 < color_count)
		{
			tables.erase(key(collection, clut));
			return;
		}
	}
	else
	{
		// Alternate table: the primary's runs, remapped (8-bit copies the
		// primary tables and remaps them)
		auto primary = tables.find(key(collection, 0));
		if (primary == tables.end())
		{
			tables.erase(key(collection, clut));
			return;
		}
		std::memcpy(r.data[0], primary->second.data[0], sizeof(r.data[0]));
	}

	for (int p = 0; p < 256; ++p)
	{
		const int c = remap ? remap[p] : p;
		uint8_t* out = r.data[1][p];
		if (c < color_count)
		{
			out[0] = colors[c].red >> 8;
			out[1] = colors[c].green >> 8;
			out[2] = colors[c].blue >> 8;
		}
		else
			out[0] = out[1] = out[2] = 0;
		out[3] = 255;
	}
	DebugDump(collection, clut);
}

void DebugDump(short collection, short clut)
{
	const char* path = std::getenv("DURANDAL_DUMP_RAMPS");
	if (!path) return;
	auto it = tables.find(key(collection, clut));
	if (it == tables.end()) return;
	FILE* f = std::fopen(path, "a");
	std::fprintf(f, "collection %d clut %d\n", collection, clut);
	for (int b = 0; b < 256; ++b) {
		const uint8_t* i = it->second.data[0][b];
		if (!i[2]) continue;
		const uint8_t* c = it->second.data[1][b];
		std::fprintf(f, "  %3d: start %3d i %2d count %2d flags %02x  rgb %3d %3d %3d\n", b, i[0], i[1], i[2], i[3], c[0], c[1], c[2]);
	}
	std::fclose(f);
}

const Ramps* Get(short collection, short clut)
{
	auto it = tables.find(key(collection, clut));
	return it == tables.end() ? nullptr : &it->second;
}

uint32_t Generation()
{
	return generation;
}

}
