#ifndef DURANDAL_SHADING_H
#define DURANDAL_SHADING_H
/*
	DurandalShading.h — Durandal project

	Marathon's 8-bit shading tables in true colour (roadmap L1).

	In 8-bit colour, Marathon darkens a pixel by walking along the colour
	ramp it belongs to in the colour table (runs of ever-darker colours the
	artists laid out) towards black, not by scaling its RGB: a darker shade
	is a different, authored colour, so hues shift as light falls. The 16-
	and 32-bit tables (and the OpenGL renderers) scale RGB instead.

	shapes.cpp calls Build() for every colour table with exactly the colour
	list, run structure and remapping its own 8-bit tables use
	(build_shading_tables8 and the alternate-table remapping); the Metal
	world renderer then evaluates the walk per texel, either banded (the
	32 tables, as the 8-bit renderer) or smoothly between ramp colours.

	Ramps layout, 256 x 2 RGBA8:
	  row 0, per pixel value: run start, index within the run, run length,
	         flags (bit 0: the run fades to black one step past its end;
	         bit 7: self-luminous colour, which only walks halfway);
	  row 1, per palette position: the colour (8 bits per channel).

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include <cstdint>

struct rgb_color_value;

namespace DurandalShading {

// 8-bit Marathon's number of shading tables
constexpr int kLevels = 32;

struct Ramps {
	uint8_t data[2][256][4];
};

// shapes.cpp: a new colour environment is being built (level change)
void BeginColorEnvironment();

// shapes.cpp: colour table `clut` of `collection`. `colors`/`color_count`
// are the list the shading tables are built from; `remap` is the
// alternate-table remapping (palette position -> colour), null for the
// primary table, whose run structure alternates reuse, as in 8-bit.
void Build(short collection, short clut, const rgb_color_value* colors, short color_count, const uint8_t* remap);

// Null if the table has not been built.
const Ramps* Get(short collection, short clut);

// Changes whenever the colour environment is rebuilt.
uint32_t Generation();

}

#endif
