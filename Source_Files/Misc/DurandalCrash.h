/*
	DurandalCrash.h — Durandal project

	A crash catcher for play sessions. Xcode's Run swallows the crash report
	macOS would otherwise write, so on a fatal signal (or an uncaught C++
	exception) the game appends what it can to
	~/Library/Logs/Durandal Crash.txt: the signal and address, what the
	main thread was doing (Phase), the game state, level, tick and player
	position, memory in use, and a backtrace with the binary's load address
	so it can be symbolicated later (atos -o Durandal -l <load address>).
	Then the signal proceeds as it would have. A diary line goes to the
	Aleph One log every five minutes (Heartbeat), so a session's history is
	known when it ends badly.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#ifndef DURANDAL_CRASH_H
#define DURANDAL_CRASH_H

namespace DurandalCrash {

// Installs the handlers; call once, early in main()
void Install();

// What the main thread is about to do, for the crash record: a string
// literal (the pointer is kept, not the text)
void Phase(const char* what);

// Called from the main loop; writes the diary line when five minutes
// have passed, and notes the longest gap between calls
void Heartbeat();

// Memory in use, megabytes: resident pages, and the physical footprint
// (what Activity Monitor shows: resident plus compressed, less shared)
unsigned long ResidentMB();
unsigned long FootprintMB();

}

#endif
