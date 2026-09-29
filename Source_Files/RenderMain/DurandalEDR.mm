/*
	DurandalEDR.mm — Durandal project

	The display's extended dynamic range headroom for the Durandal window,
	and one AppKit setting for development runs. Kept apart from the engine's headers: AppKit's legacy types clash with
	cseries.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#import <AppKit/AppKit.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

float DurandalEDR_Headroom(SDL_Window* window)
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (!window || !SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_COCOA)
		return 1.0f;
	NSWindow* w = info.info.cocoa.window;
	NSScreen* screen = w.screen ? w.screen : NSScreen.mainScreen;
	return screen ? float(screen.maximumExtendedDynamicRangeColorComponentValue) : 1.0f;
}

// The refresh rate of the display the window is on (0 if unknown)
float DurandalEDR_RefreshRate(SDL_Window* window)
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (!window || !SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_COCOA)
		return 0.0f;
	NSWindow* w = info.info.cocoa.window;
	NSScreen* screen = w.screen ? w.screen : NSScreen.mainScreen;
	return screen ? float(screen.maximumFramesPerSecond) : 0.0f;
}

// Development runs (hidden benchmark, menu shots): never offer to reopen
// windows after an earlier crash. That prompt is modal and would block the
// hidden run. In memory only (the registration domain); nothing is saved.
void DurandalEDR_IgnoreSavedState()
{
	if (!getenv("DURANDAL_BENCHMARK_HIDDEN") && !getenv("DURANDAL_MENU_SHOT"))
		return;
	[[NSUserDefaults standardUserDefaults] registerDefaults:@{ @"ApplePersistenceIgnoreState": @YES }];
}
