#ifndef DURANDAL_TERMINAL_H
#define DURANDAL_TERMINAL_H
/*
	DurandalTerminal.h — Durandal project

	Crisp terminals (roadmap T1). Terminals are drawn into a 640x320 image
	that is then stretched to the screen, so their text is blurred or
	blocky at modern sizes. With Crisp Terminals on, the terminal is still
	laid out and drawn at 640x320 exactly as before (so page breaks and
	positions cannot change), except that TrueType text drawn into it is
	recorded instead. Compose() then scales that image up by a whole number
	(pixel art, static and borders stay sharp) and draws the recorded text
	on top from the same fonts at the larger size.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "cseries.h"
#include <SDL2/SDL.h>
#include <cstddef>

class ttf_font_info;
struct screen_rectangle;

namespace DurandalTerminal {

// computer_interface.cpp: one redraw of the terminal into `target`
// (Term_Buffer); text drawn into it meanwhile is recorded when enabled.
class Redraw {
public:
	explicit Redraw(SDL_Surface* target);
	~Redraw();
};

// screen_drawing.cpp (ttf_font_info::_draw_text): whether text drawn into
// `s` should be recorded rather than drawn, and recording it.
bool Recording(SDL_Surface* s);
void Record(const ttf_font_info* font, const char* text, size_t length, int x, int y, uint32 pixel,
			uint16 style, bool utf8, const screen_rectangle* clip, const SDL_PixelFormat* format);

// screen.cpp: the image to show for `base` (Term_Buffer) in a rectangle
// `display_height` pixels tall: `base` itself, or the crisp version.
SDL_Surface* Compose(SDL_Surface* base, int display_height);

}

#endif
