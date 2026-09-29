/*
	DurandalCrash.cpp — Durandal project

	See DurandalCrash.h. The handler keeps to what is safe after a fault:
	no allocation, no stdio, no locks; numbers are written with a small
	formatter of its own. The game's globals are read as they are (a
	corrupt one may fault again, in which case the second fault takes the
	default action, as the handler resets itself first).

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

// System headers first: cseries.h defines macros that clash with them
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <exception>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "cseries.h"
#include "DurandalCrash.h"

#include "map.h"
#include "player.h"
#include "interface.h"
#include "Logging.h"

namespace {

char crash_path[1024] = "";
const char* volatile phase = "starting";
time_t started = 0;
time_t last_heartbeat = 0;
double last_call = 0, longest_gap = 0;

// Resident memory in megabytes
unsigned long resident_mb()
{
	mach_task_basic_info info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
		return 0;
	return static_cast<unsigned long>(info.resident_size >> 20);
}

// Physical footprint in megabytes (resident plus compressed, as Activity Monitor)
unsigned long footprint_mb()
{
	task_vm_info_data_t info;
	mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
	if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
		return 0;
	return static_cast<unsigned long>(info.phys_footprint >> 20);
}

// Async-signal-safe output
void put(int fd, const char* s)
{
	if (s)
		write(fd, s, strlen(s));
}

void put_number(int fd, long long v)
{
	char buffer[32];
	int n = 0;
	bool negative = v < 0;
	unsigned long long u = negative ? -(unsigned long long)v : (unsigned long long)v;
	do { buffer[n++] = char('0' + u % 10); u /= 10; } while (u && n < 30);
	if (negative) buffer[n++] = '-';
	for (int i = 0; i < n / 2; ++i) std::swap(buffer[i], buffer[n - 1 - i]);
	write(fd, buffer, n);
}

void put_hex(int fd, unsigned long long v)
{
	char buffer[20];
	int n = 0;
	do { const int d = v & 15; buffer[n++] = char(d < 10 ? '0' + d : 'a' + d - 10); v >>= 4; } while (v && n < 18);
	for (int i = 0; i < n / 2; ++i) std::swap(buffer[i], buffer[n - 1 - i]);
	put(fd, "0x");
	write(fd, buffer, n);
}

void write_record(int fd, const char* title, int signal_number, void* address)
{
	put(fd, "\n==== ");
	put(fd, title);
	put(fd, " at ");
	put_number(fd, static_cast<long long>(time(nullptr)));
	put(fd, " (unix time), ");
	put_number(fd, static_cast<long long>(time(nullptr) - started));
	put(fd, " s into the session\n");
	if (signal_number)
	{
		put(fd, "signal ");
		put_number(fd, signal_number);
		put(fd, " address ");
		put_hex(fd, reinterpret_cast<unsigned long long>(address));
		put(fd, "\n");
	}
	put(fd, "phase: ");
	put(fd, phase);
	put(fd, "\ngame state: ");
	put_number(fd, get_game_state());
	if (dynamic_world)
	{
		put(fd, "\nlevel ");
		put_number(fd, dynamic_world->current_level_number);
		put(fd, " tick ");
		put_number(fd, dynamic_world->tick_count);
		if (static_world)
		{
			put(fd, " \"");
			char name[LEVEL_NAME_LENGTH + 1];
			strncpy(name, static_world->level_name, LEVEL_NAME_LENGTH);
			name[LEVEL_NAME_LENGTH] = 0;
			put(fd, name);
			put(fd, "\"");
		}
		if (local_player)
		{
			put(fd, "\nplayer polygon ");
			put_number(fd, local_player->supporting_polygon_index);
			put(fd, " at ");
			put_number(fd, local_player->location.x);
			put(fd, ", ");
			put_number(fd, local_player->location.y);
			put(fd, ", ");
			put_number(fd, local_player->location.z);
			put(fd, " facing ");
			put_number(fd, local_player->facing);
			put(fd, " elevation ");
			put_number(fd, local_player->elevation);
		}
	}
	put(fd, "\nresident memory MB ");
	put_number(fd, static_cast<long long>(resident_mb()));
	put(fd, "\nlongest gap between main loop passes ms ");
	put_number(fd, static_cast<long long>(longest_gap * 1000));
	put(fd, "\nload address ");
	put_hex(fd, reinterpret_cast<unsigned long long>(_dyld_get_image_header(0)));
	put(fd, " (symbolicate: atos -o <path to Durandal binary> -l <load address> <frame addresses>)\nbacktrace:\n");
	void* frames[64];
	const int count = backtrace(frames, 64);
	backtrace_symbols_fd(frames, count, fd);
	put(fd, "==== end\n");
}

int open_record()
{
	return crash_path[0] ? open(crash_path, O_WRONLY | O_APPEND | O_CREAT, 0644) : -1;
}

void on_signal(int signal_number, siginfo_t* info, void*)
{
	// Take the default action on any fault while recording
	signal(signal_number, SIG_DFL);
	const int fd = open_record();
	if (fd >= 0)
	{
		write_record(fd, "Durandal crashed", signal_number, info ? info->si_addr : nullptr);
		close(fd);
	}
	raise(signal_number);
}

void on_terminate()
{
	const int fd = open_record();
	if (fd >= 0)
	{
		write_record(fd, "Durandal: uncaught C++ exception", 0, nullptr);
		close(fd);
	}
	abort();
}

// Development: DURANDAL_CRASH_TEST=<seconds> faults on purpose after that
// long, to check the record is written
int crash_test_seconds = 0;

}

unsigned long DurandalCrash::ResidentMB() { return resident_mb(); }
unsigned long DurandalCrash::FootprintMB() { return footprint_mb(); }

void DurandalCrash::Install()
{
	started = time(nullptr);
	last_heartbeat = started;
	if (const char* home = std::getenv("HOME"))
		snprintf(crash_path, sizeof(crash_path), "%s/Library/Logs/Durandal Crash.txt", home);
	if (const char* test = std::getenv("DURANDAL_CRASH_TEST"))
		crash_test_seconds = std::atoi(test);

	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_sigaction = on_signal;
	action.sa_flags = SA_SIGINFO | SA_RESETHAND;
	sigemptyset(&action.sa_mask);
	for (int s : { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP })
		sigaction(s, &action, nullptr);
	std::set_terminate(on_terminate);
}

void DurandalCrash::Phase(const char* what)
{
	phase = what;
}

void DurandalCrash::Heartbeat()
{
	timeval tv;
	gettimeofday(&tv, nullptr);
	const double now = tv.tv_sec + tv.tv_usec / 1e6;
	if (last_call > 0 && now - last_call > longest_gap)
		longest_gap = now - last_call;
	last_call = now;

	if (crash_test_seconds > 0 && tv.tv_sec - started >= crash_test_seconds)
	{
		crash_test_seconds = 0;
		logNote("Durandal: DURANDAL_CRASH_TEST faulting on purpose");
		volatile int* nowhere = reinterpret_cast<volatile int*>(0x1518);
		*nowhere = 1;
	}

	if (tv.tv_sec - last_heartbeat < 300)
		return;
	last_heartbeat = tv.tv_sec;
	const long minutes = (tv.tv_sec - started) / 60;
	if (dynamic_world && get_game_state() == _game_in_progress)
		logNote("Durandal: %ld min up, level %d tick %u, resident %lu MB, longest main loop gap %.0f ms",
			minutes, int(dynamic_world->current_level_number), unsigned(dynamic_world->tick_count), resident_mb(), longest_gap * 1000);
	else
		logNote("Durandal: %ld min up, game state %d, resident %lu MB, longest main loop gap %.0f ms",
			minutes, int(get_game_state()), resident_mb(), longest_gap * 1000);
	longest_gap = 0;
}
