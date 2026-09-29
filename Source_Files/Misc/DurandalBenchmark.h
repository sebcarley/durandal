#ifndef DURANDAL_BENCHMARK_H
#define DURANDAL_BENCHMARK_H
/*
	DurandalBenchmark.h — Durandal project

	Command-line frame-time benchmark. Plays a film unattended, logs every
	presented in-game frame to a CSV, writes a summary and quits.

	  <app> --benchmark out.csv [--benchmark-size 1920x1080|native]
	        [--benchmark-fps 0|30|60|120] -s film.filA

	All preference changes it makes are in memory only: while it is active,
	write_preferences() does nothing, so the player's settings are untouched.
	It reads game state but never writes it, so films stay deterministic.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include <cstdint>
#include <string>
#include <vector>

namespace DurandalBenchmark {

// True when --benchmark was given.
bool Active();

// True for any unattended development run (benchmark or menu shot); such
// runs never write preferences.
bool DevRun();

// Writes RGBA8 rows (top-down) as a PNG.
bool SavePNG(const std::string& path, std::vector<uint8_t>& rgba, int w, int h);

// DURANDAL_MENU_SHOT=<png>: after three seconds on the main menu, save the
// finished frame there and quit. Call before the buffer swap.
void MenuShotBeforeSwap();
// screen.cpp: a terminal page was drawn; with DURANDAL_BENCHMARK_TERMINAL_SHOTS
// set, the frame is saved as a frame shot named ...-terminal-...
void TerminalPageShown();
// Main loop, development only: with DURANDAL_MENU_SHOT_DIALOG=durandal, opens
// the DURANDAL preferences dialog at the main menu, so the menu shot shows it.
void MainLoopHook();

// Applies the in-memory preference overrides (window size, uncapped frame
// rate, no vsync) and switches on unattended film playback. Call once,
// after preferences are loaded and before the screen is created.
void ApplyOverrides();

// Records one presented frame. Call straight after the buffer swap.
void FrameRendered(double heartbeat_fraction);

// Writes the CSV and summary. Called when the film ends.
void Finish();

// Development timing: where the CPU spends each frame, recorded with the
// frame (CSV columns wait_ms, tick_ms, render_ms, present_ms). Wait: for
// the display (the next drawable, frames in flight); Tick: update_world();
// Render: render_screen() up to the swap; Present: the swap itself. Waiting
// during the render stage counts as waiting, not rendering.
enum CpuStage { kStageWait, kStageTick, kStageRender, kStagePresent, kStageCount };
void StageBegin(CpuStage stage);
void StageEnd(CpuStage stage);
// Per-frame counts recorded with the frame: dynamic lights, contact casters.
void FrameCounts(int light_count, int caster_count);
// The index the next recorded frame will have; GPU timing rows carry it, so
// the two logs join exactly.
size_t NextFrameIndex();
// DURANDAL_BENCHMARK_END_TICK=<n>: the run stops after game tick n (a short
// run over one stretch of a film); the summary covers what was played.

// --benchmark-hidden: the window is created hidden and the mouse is left
// alone, for unattended development runs that must not disturb the desk.
bool HiddenWindow();

// Parity check (--benchmark-shots): true when a shot is due this frame.
bool ShotDue();
// Renders the world once with OpenGL and once with Metal through
// `render_world`, saves both and a difference image, and logs the score.
void ParityShot(void (*render_world)());

// Full-frame shots (DURANDAL_BENCHMARK_FRAMESHOTS=dir): the finished frame,
// HUD included, read back just before it is presented, so separate OpenGL
// and Metal-display runs can be compared at the same film ticks. Call
// FrameShotDue() before the swap; SaveFrameShot() takes top-down RGBA.
bool FrameShotDue();
void SaveFrameShot(std::vector<uint8_t>& rgba, int width, int height);

}

#endif
