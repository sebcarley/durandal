/*
	DurandalTerminal.cpp — Durandal project

	See DurandalTerminal.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "DurandalTerminal.h"

#include "DurandalPreferences.h"
#include "DurandalGL.h"
#include "screen_drawing.h"
#include "sdl_fonts.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace DurandalTerminal {

namespace {

struct Text {
	const ttf_font_info* font;
	std::string text;
	int x, y;
	SDL_Color color;
	uint16 style;
	bool utf8;
	bool clipped;
	screen_rectangle clip;
};

SDL_Surface* recording_target = nullptr;
bool recording = false;
std::vector<Text> texts;		// from the last redraw
SDL_Surface* crisp = nullptr;	// Compose()'s image, reused

// Metal display only: OpenGL's image blitter tiles large images, and
// scaling the composed terminal down would show the seams
bool enabled()
{
	return DurandalGL::Active() && Durandal::Enabled(Durandal::kCrispTerminals);
}

}

Redraw::Redraw(SDL_Surface* target)
{
	texts.clear();
	recording_target = target;
	recording = enabled();
}

Redraw::~Redraw()
{
	recording = false;
}

bool Recording(SDL_Surface* s)
{
	return recording && s && s == recording_target;
}

void Record(const ttf_font_info* font, const char* text, size_t length, int x, int y, uint32 pixel,
			uint16 style, bool utf8, const screen_rectangle* clip, const SDL_PixelFormat* format)
{
	Text t;
	t.font = font;
	t.text.assign(text, length);
	t.x = x;
	t.y = y;
	SDL_GetRGB(pixel, format, &t.color.r, &t.color.g, &t.color.b);
	t.color.a = 0xff;
	t.style = style;
	t.utf8 = utf8;
	t.clipped = clip != nullptr;
	if (clip)
		t.clip = *clip;
	texts.push_back(std::move(t));
}

SDL_Surface* Compose(SDL_Surface* base, int display_height)
{
	// Without recorded text the image is complete as it is
	if (!base || base != recording_target || texts.empty())
		return base;
	// Recorded text must still be drawn if the setting was switched off since
	const int k = enabled() ? std::clamp(int(std::ceil(display_height / float(base->h))), 1, 4) : 1;

	const int w = base->w * k, h = base->h * k;
	if (!crisp || crisp->w != w || crisp->h != h || crisp->format->format != base->format->format)
	{
		if (crisp)
			SDL_FreeSurface(crisp);
		crisp = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, base->format->BitsPerPixel, base->format->Rmask,
									 base->format->Gmask, base->format->Bmask, base->format->Amask);
		if (!crisp)
			return base;
	}

	// Everything but the text, whole-number scaled so it stays sharp
	SDL_SetSurfaceBlendMode(base, SDL_BLENDMODE_NONE);
	SDL_Rect dst = { 0, 0, w, h };
	SDL_BlitScaled(base, nullptr, crisp, &dst);

	// The text, from the same fonts at k times the size
	for (const Text& t : texts)
	{
		screen_rectangle clip = t.clip;
		if (t.clipped)
		{
			clip.top *= k;
			clip.left *= k;
			clip.bottom *= k;
			clip.right *= k;
		}
		t.font->durandal_draw_scaled(crisp, t.text.c_str(), t.text.size(), t.x * k, t.y * k, t.color, t.style,
									 t.utf8, k, t.clipped ? &clip : nullptr);
	}
	return crisp;
}

}
