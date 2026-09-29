#ifndef DURANDAL_RELIEF_H
#define DURANDAL_RELIEF_H
/*
	DurandalRelief.h — Durandal project

	Surface relief (roadmap M1). Marathon's wall, floor and ceiling art is
	painted with its relief in it: bright ridges, dark seams and pits. A
	height map derived from each texture's brightness (lightly blurred,
	contrast set per texture) lets the renderer give that relief a shape
	under light that has a direction: the headlight and the dynamic lights
	(E2). Surfaces lit only by their room's light look as before, since
	that light has no direction. Reviewed wall lights (DurandalGlow:
	screens, liquids, lamps) stay flat.

	The heights ride in the unused fourth channel of the 8-bit shading's
	index texture (TextureManager::PlaceIndexImage), so they follow its
	mip chain; the true-colour path has no relief.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include <cstddef>
#include <cstdint>

namespace DurandalRelief {

// Fills channel 3 of `texels` (RGBA: colour index, opacity, glow, height;
// `width` x `height`, tiling) with heights 1-255 from the brightness of
// each texel's colour in `colours` (RGBA8 per index), or 0 (no relief)
// throughout when `flat`.
void Derive(uint8_t* texels, int width, int height, const uint32_t* colours, bool flat);

}

#endif
