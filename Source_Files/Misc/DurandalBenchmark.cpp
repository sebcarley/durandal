/*
	DurandalBenchmark.cpp — Durandal project

	See DurandalBenchmark.h. Reads game state only; never touches the game
	RNG or world arrays, so film playback is unaffected.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "DurandalBenchmark.h"

#include "shell_options.h"
#include "preferences.h"
#include "interface.h"
#include "map.h"
#include "screen.h"
#include "Logging.h"
#include "DurandalMetal.h"
#include "DurandalGL.h"
#include "DurandalPreferences.h"
#include "DurandalCrash.h"

#include <SDL2/SDL_image.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

extern bool displaying_fps;	// screen_shared.h

namespace {

using Clock = std::chrono::steady_clock;

struct Frame {
	double time;       // seconds since the first recorded frame
	int32 tick;        // game tick being shown
	double fraction;   // heartbeat fraction (1.0 = on the tick)
	double stage[DurandalBenchmark::kStageCount];	// CPU ms per stage (development timing)
	int light_count;   // dynamic lights this frame
	int caster_count;  // contact shadow casters this frame
};

// Frames in the first WARM_UP seconds (level load, texture upload) are
// logged to the CSV but left out of the summary.
const double WARM_UP = 2.0;

std::vector<Frame> frames;
Clock::time_point start;
bool finished = false;
int pixel_width = 0, pixel_height = 0;
int32 next_shot_tick = 0;
int32 next_frame_shot_tick = 0;
bool speed_applied = false;

// Development timing (StageBegin/StageEnd), reset as each frame is recorded
double stage_ms[DurandalBenchmark::kStageCount];
Clock::time_point stage_start[DurandalBenchmark::kStageCount];
bool stage_pending[DurandalBenchmark::kStageCount];
double wait_inside_render = 0;
int frame_lights = 0, frame_casters = 0;
int32 end_tick = 0;			// DURANDAL_BENCHMARK_END_TICK
bool stop_requested = false;

bool save_png(const std::string& path, std::vector<uint8_t>& rgba, int w, int h)
{
	SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(rgba.data(), w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
	if (!s)
		return false;
	const bool ok = IMG_SavePNG(s, path.c_str()) == 0;
	SDL_FreeSurface(s);
	return ok;
}

}

namespace DurandalBenchmark {

bool Active()
{
	return !shell_options.benchmark_log.empty();
}

void ApplyOverrides()
{
	if (!DevRun())
		return;

	if (!Active())
	{
		// Menu shot: just a fixed-size window (never the player's full screen)
		auto& mode = graphics_preferences->screen_mode;
		int w = 1280, h = 720;
		sscanf(shell_options.benchmark_size.c_str(), "%dx%d", &w, &h);
		mode.fullscreen = false;
		mode.high_dpi = false;
		mode.auto_resolution = false;
		mode.width = w;
		mode.height = h;
		return;
	}

	// Reuse the unattended film mode the film tests use: no menus, no
	// chapter screens, no pause on focus loss, quit when the film ends.
	if (shell_options.replay_directory.empty())
		shell_options.replay_directory = "benchmark";

	graphics_preferences->fps_target = shell_options.benchmark_fps.empty() ? 0 : std::atoi(shell_options.benchmark_fps.c_str());
	// DURANDAL_BENCHMARK_VSYNC=1: vsync on, as in play (off by default)
	graphics_preferences->OGL_Configure.WaitForVSync = std::getenv("DURANDAL_BENCHMARK_VSYNC") && std::atoi(std::getenv("DURANDAL_BENCHMARK_VSYNC")) != 0;

	auto& mode = graphics_preferences->screen_mode;
	int w = 0, h = 0;
	if (sscanf(shell_options.benchmark_size.c_str(), "%dx%d", &w, &h) == 2 && w > 0 && h > 0)
	{
		// Windowed at exactly w x h drawable pixels.
		mode.fullscreen = false;
		mode.high_dpi = false;
		mode.auto_resolution = false;
		mode.width = w;
		mode.height = h;
	}
	else
	{
		// "native": full screen at the display's full backing resolution.
		mode.fullscreen = true;
		mode.high_dpi = true;
	}

	// Metal display: record when each frame actually reached the screen
	DurandalGL::SetCollectPresentTimes(true);

	// DURANDAL_BENCHMARK_RENDERER=metal|gl forces the world renderer
	if (const char* r = std::getenv("DURANDAL_BENCHMARK_RENDERER"))
		DurandalMetal::ForceWorldRenderer(std::string(r) == "metal" ? 1 : 0);

	// DURANDAL_BENCHMARK_SHOW_FPS=1: the on-screen fps counter, so frame
	// comparisons also cover in-game text
	if (std::getenv("DURANDAL_BENCHMARK_SHOW_FPS"))
		displaying_fps = true;

	// DURANDAL_BENCHMARK_END_TICK=<n>: stop after that game tick
	if (const char* e = std::getenv("DURANDAL_BENCHMARK_END_TICK"))
		end_tick = std::atoi(e);

	frames.reserve(1 << 17);
}

bool SavePNG(const std::string& path, std::vector<uint8_t>& rgba, int w, int h)
{
	return save_png(path, rgba, w, h);
}

bool DevRun()
{
	return Active() || std::getenv("DURANDAL_MENU_SHOT");
}

bool HiddenWindow()
{
	return DevRun() && shell_options.benchmark_hidden;
}

void MenuShotBeforeSwap()
{
	static const char* path = std::getenv("DURANDAL_MENU_SHOT");
	static uint64_t first = 0;
	if (!path || get_game_state() != _display_main_menu)
		return;
	if (!first)
		first = machine_tick_count();
	if (machine_tick_count() - first < 3000)
		return;
	const int w = MainScreenPixelWidth(), h = MainScreenPixelHeight();
	std::vector<uint8_t> rgba(size_t(w) * h * 4), flipped(rgba.size());
	DurandalGL::ReadFramebuffer(w, h, rgba.data());
	for (int y = 0; y < h; ++y)	// GL rows run bottom-up
		std::copy(rgba.begin() + size_t(h - 1 - y) * w * 4, rgba.begin() + size_t(h - y) * w * 4, flipped.begin() + size_t(y) * w * 4);
	save_png(path, flipped, w, h);
	std::exit(0);
}

void MainLoopHook()
{
	// A short run (DURANDAL_BENCHMARK_END_TICK) ends the way the film's end
	// does, from the top of the main loop
	if (stop_requested && get_game_state() == _game_in_progress)
	{
		stop_requested = false;
		set_game_state(_close_game);
	}
	static const char* dialog = std::getenv("DURANDAL_MENU_SHOT_DIALOG");
	static bool opened = false;
	if (!dialog || opened || !std::getenv("DURANDAL_MENU_SHOT") || get_game_state() != _display_main_menu)
		return;
	opened = true;
	if (std::string(dialog) == "durandal")
		Durandal::Dialog(nullptr);

}

bool ShotDue()
{
	return Active() && !shell_options.benchmark_shots.empty() && !finished &&
		get_game_state() == _game_in_progress && dynamic_world->tick_count >= next_shot_tick;
}

static bool terminal_page_shown = false;

void TerminalPageShown()
{
	static const bool wanted = std::getenv("DURANDAL_BENCHMARK_TERMINAL_SHOTS") != nullptr;
	if (wanted)
		terminal_page_shown = true;
}

bool FrameShotDue()
{
	static const char* dir = std::getenv("DURANDAL_BENCHMARK_FRAMESHOTS");
	if (!Active() || !dir || finished || get_game_state() != _game_in_progress)
		return false;
	return dynamic_world->tick_count >= next_frame_shot_tick || terminal_page_shown;
}

void SaveFrameShot(std::vector<uint8_t>& rgba, int w, int h)
{
	const int every = shell_options.benchmark_shot_every.empty() ? 600 : std::max(1, std::atoi(shell_options.benchmark_shot_every.c_str()));
	const int32 tick = dynamic_world->tick_count;
	const bool terminal = terminal_page_shown;
	terminal_page_shown = false;
	char name[96];
	if (terminal)
		snprintf(name, sizeof(name), "/tick%06d-terminal-%s.png", tick, DurandalMetal::DisplayActive() ? "metal" : "gl");
	else
	{
		next_frame_shot_tick = (tick / every + 1) * every;	// same ticks in every run
		snprintf(name, sizeof(name), "/tick%06d-frame-%s.png", tick, DurandalMetal::DisplayActive() ? "metal" : "gl");
	}
	save_png(std::string(std::getenv("DURANDAL_BENCHMARK_FRAMESHOTS")) + name, rgba, w, h);
}

void ParityShot(void (*render_world)())
{
	const int every = shell_options.benchmark_shot_every.empty() ? 600 : std::max(1, std::atoi(shell_options.benchmark_shot_every.c_str()));
	const int32 tick = dynamic_world->tick_count;
	next_shot_tick = tick + every;

	std::vector<uint8_t> image[2];
	int w[2] = {0, 0}, h[2] = {0, 0};
	for (int metal = 0; metal < 2; ++metal)
	{
		DurandalMetal::ForceWorldRenderer(metal);
		DurandalMetal::RequestCapture();
		render_world();
		if (!DurandalMetal::TakeCapture(image[metal], w[metal], h[metal]))
			logWarning("Durandal parity: no %s capture at tick %d", metal ? "Metal" : "OpenGL", tick);
	}
	DurandalMetal::ForceWorldRenderer(-1);

	const std::string& dir = shell_options.benchmark_shots;
	char name[64];
	snprintf(name, sizeof(name), "/tick%06d", tick);
	if (!image[0].empty())
		save_png(dir + name + "-gl.png", image[0], w[0], h[0]);
	if (!image[1].empty())
		save_png(dir + name + "-metal.png", image[1], w[1], h[1]);

	FILE* csv = fopen((dir + "/parity.csv").c_str(), "a");
	if (!csv)
		return;
	if (image[0].empty() || image[1].empty() || w[0] != w[1] || h[0] != h[1])
	{
		fprintf(csv, "%d,%d,%d,%d,%d,missing or size mismatch,,\n", tick, w[0], h[0], w[1], h[1]);
		fclose(csv);
		return;
	}

	// Score: mean absolute difference (0-255), share of pixels differing
	// by more than 16 in any channel, largest difference; and an
	// amplified difference image.
	std::vector<uint8_t> diff(image[0].size());
	double total = 0;
	size_t over = 0;
	int largest = 0;
	const size_t pixels = size_t(w[0]) * h[0];
	for (size_t i = 0; i < pixels; ++i)
	{
		int worst = 0;
		for (int c = 0; c < 3; ++c)
		{
			const int d = std::abs(int(image[0][i * 4 + c]) - int(image[1][i * 4 + c]));
			total += d;
			worst = std::max(worst, d);
		}
		largest = std::max(largest, worst);
		if (worst > 16)
			++over;
		const uint8_t v = uint8_t(std::min(255, worst * 4));
		diff[i * 4] = v; diff[i * 4 + 1] = v; diff[i * 4 + 2] = v; diff[i * 4 + 3] = 255;
	}
	save_png(dir + name + "-diff.png", diff, w[0], h[0]);
	fprintf(csv, "%d,%d,%d,%.3f,%.3f,%d\n", tick, w[0], h[0], total / (pixels * 3), 100.0 * over / pixels, largest);
	fclose(csv);
}

void FrameRendered(double heartbeat_fraction)
{
	if (!Active() || finished || get_game_state() != _game_in_progress)
		return;

	if (!speed_applied && !shell_options.benchmark_speed.empty())
	{
		set_replay_speed(std::atoi(shell_options.benchmark_speed.c_str()));
		speed_applied = true;
	}

	auto now = Clock::now();
	if (frames.empty())
	{
		start = now;
		pixel_width = MainScreenPixelWidth();
		pixel_height = MainScreenPixelHeight();
	}
	Frame fr{};
	fr.time = std::chrono::duration<double>(now - start).count();
	fr.tick = dynamic_world->tick_count;
	fr.fraction = heartbeat_fraction;
	std::copy(std::begin(stage_ms), std::end(stage_ms), std::begin(fr.stage));
	fr.light_count = frame_lights;
	fr.caster_count = frame_casters;
	frames.push_back(fr);
	std::fill(std::begin(stage_ms), std::end(stage_ms), 0.0);
	wait_inside_render = 0;

	if (end_tick > 0 && dynamic_world->tick_count >= end_tick && !stop_requested)
	{
		Finish();
		stop_requested = true;
	}
}

void StageBegin(CpuStage stage)
{
	if (!Active())
		return;
	stage_start[stage] = Clock::now();
	stage_pending[stage] = true;
}

void StageEnd(CpuStage stage)
{
	if (!Active() || !stage_pending[stage])
		return;
	stage_pending[stage] = false;
	const double ms = std::chrono::duration<double, std::milli>(Clock::now() - stage_start[stage]).count();
	if (stage == kStageWait && stage_pending[kStageRender])
		wait_inside_render += ms;	// the in-flight wait at the frame's first draw
	if (stage == kStageRender)
	{
		stage_ms[stage] += ms - wait_inside_render;
		wait_inside_render = 0;
	}
	else
		stage_ms[stage] += ms;
}

void FrameCounts(int light_count, int caster_count)
{
	frame_lights = light_count;
	frame_casters = caster_count;
}

size_t NextFrameIndex()
{
	return frames.size();
}

void Finish()
{
	if (!Active() || finished)
		return;
	finished = true;

	const std::string& path = shell_options.benchmark_log;
	if (FILE* csv = fopen(path.c_str(), "w"))
	{
		fprintf(csv, "frame,time_s,dt_ms,tick,fraction,wait_ms,tick_ms,render_ms,present_ms,lights,casters\n");
		for (size_t i = 0; i < frames.size(); ++i)
		{
			double dt = i ? (frames[i].time - frames[i - 1].time) * 1000.0 : 0.0;
			const Frame& fr = frames[i];
			fprintf(csv, "%zu,%.6f,%.3f,%d,%.4f,%.3f,%.3f,%.3f,%.3f,%d,%d\n", i, fr.time, dt, fr.tick, fr.fraction,
					fr.stage[kStageWait], fr.stage[kStageTick], fr.stage[kStageRender], fr.stage[kStagePresent], fr.light_count, fr.caster_count);
		}
		fclose(csv);
	}

	// Summary over the frames after the warm-up.
	std::vector<Frame> f;
	for (const auto& fr : frames)
		if (fr.time >= WARM_UP)
			f.push_back(fr);

	std::ostringstream s;
	s << "Durandal benchmark\n"
	  << "  drawable: " << pixel_width << " x " << pixel_height << " px\n"
	  << "  renderer: " << (graphics_preferences->screen_mode.acceleration == _no_acceleration ? "software" :
			DurandalMetal::DisplayActive() ? (std::getenv("DURANDAL_BENCHMARK_OFFSCREEN") ? "Metal display (off-screen, not presented)" : "Metal display") :
			DurandalMetal::WorldRendererActive() ? "Metal world (bridge) + OpenGL overlays" : "OpenGL (shader)") << "\n"
	  << "  fps target: " << graphics_preferences->fps_target << " (0 = uncapped), vsync "
	  << (graphics_preferences->OGL_Configure.WaitForVSync ? "on" : "off");
	if (DurandalGL::DrawableCount() > 0)
		s << ", " << DurandalGL::DrawableCount() << " drawables";
	s << "\n"
	  << "  memory at the end: " << DurandalCrash::ResidentMB() << " MB resident, "
	  << DurandalCrash::FootprintMB() << " MB footprint\n"
	  << "  frames logged: " << frames.size() << " (summary excludes first " << WARM_UP << " s)\n";

	if (f.size() < 2)
	{
		s << "  not enough frames for a summary\n";
	}
	else
	{
		double duration = f.back().time - f.front().time;
		std::vector<double> dts;
		for (size_t i = 1; i < f.size(); ++i)
			dts.push_back((f[i].time - f[i - 1].time) * 1000.0);
		std::vector<double> sorted = dts;
		std::sort(sorted.begin(), sorted.end());
		double p99 = sorted[std::min(sorted.size() - 1, static_cast<size_t>(sorted.size() * 0.99))];
		double worst_dt = sorted.back();

		// Worst second: fewest frames presented in any one-second window
		// starting at a frame.
		size_t worst_count = SIZE_MAX;
		double worst_at = 0;
		size_t j = 0;
		for (size_t i = 0; i < f.size() && f[i].time + 1.0 <= f.back().time; ++i)
		{
			if (j < i) j = i;
			while (j < f.size() && f[j].time < f[i].time + 1.0) ++j;
			if (j - i < worst_count)
			{
				worst_count = j - i;
				worst_at = f[i].time;
			}
		}

		// Interpolation check: frames shown between ticks, and whether any
		// (tick, fraction) state was presented twice.
		size_t between = 0;
		std::set<std::pair<int32, long>> states;
		for (const auto& fr : f)
		{
			if (fr.fraction < 0.9999) ++between;
			states.insert({fr.tick, std::lround(fr.fraction * 1e6)});
		}

		double ticks_per_s = (f.back().tick - f.front().tick) / duration;
		char buf[512];
		snprintf(buf, sizeof(buf),
			"  duration: %.1f s, game ticks/s: %.2f\n"
			"  average fps: %.1f\n"
			"  worst second: %zu frames (at %.1f s)\n"
			"  1%% low (p99 frame time): %.1f fps (%.2f ms)\n"
			"  longest frame: %.2f ms\n"
			"  frames between ticks (interpolated): %.1f%%\n"
			"  distinct world states shown: %zu of %zu frames\n",
			duration, ticks_per_s, (f.size() - 1) / duration,
			worst_count == SIZE_MAX ? f.size() : worst_count, worst_at,
			1000.0 / p99, p99, worst_dt,
			100.0 * between / f.size(), states.size(), f.size());
		s << buf;

		// Development timing: where the CPU spent the frames, and the slow ones
		double all[kStageCount + 1] = {}, slow[kStageCount + 1] = {};
		size_t slow_count = 0, slow_on_tick = 0;
		int max_lights = 0;
		double sum_lights = 0;
		for (size_t i = 1; i < f.size(); ++i)
		{
			const double dt = dts[i - 1];
			const bool is_slow = dt >= p99;
			double sum = 0;
			for (int st = 0; st < kStageCount; ++st)
			{
				all[st] += f[i].stage[st];
				sum += f[i].stage[st];
				if (is_slow) slow[st] += f[i].stage[st];
			}
			all[kStageCount] += dt - sum;
			if (is_slow)
			{
				slow[kStageCount] += dt - sum;
				++slow_count;
				if (f[i].tick != f[i - 1].tick) ++slow_on_tick;
			}
			max_lights = std::max(max_lights, f[i].light_count);
			sum_lights += f[i].light_count;
		}
		const double n = double(f.size() - 1);
		snprintf(buf, sizeof(buf),
			"  cpu per frame (ms): wait %.2f, tick %.2f, render %.2f, present %.2f, other %.2f\n"
			"  cpu in slow frames (>= p99, %zu frames, %.0f%% on a tick): wait %.2f, tick %.2f, render %.2f, present %.2f, other %.2f\n"
			"  dynamic lights: average %.1f, max %d\n",
			all[0] / n, all[1] / n, all[2] / n, all[3] / n, all[4] / n,
			slow_count, slow_count ? 100.0 * slow_on_tick / slow_count : 0.0,
			slow_count ? slow[0] / slow_count : 0.0, slow_count ? slow[1] / slow_count : 0.0, slow_count ? slow[2] / slow_count : 0.0,
			slow_count ? slow[3] / slow_count : 0.0, slow_count ? slow[4] / slow_count : 0.0,
			sum_lights / n, max_lights);
		s << buf;
	}

	// Presentation (Metal display, on screen): when frames were shown
	std::vector<double> presented;
	DurandalGL::TakePresentTimes(presented);
	presented.erase(std::remove_if(presented.begin(), presented.end(), [](double t) { return t <= 0; }), presented.end());
	std::sort(presented.begin(), presented.end());
	if (presented.size() > 2 * 60)
	{
		std::vector<double> gaps;
		for (size_t i = 1; i < presented.size(); ++i)
			if (presented[i] - presented[0] >= WARM_UP)
				gaps.push_back((presented[i] - presented[i - 1]) * 1000.0);
		if (gaps.size() > 10)
		{
			std::vector<double> g = gaps;
			std::sort(g.begin(), g.end());
			const double median = g[g.size() / 2];
			const double p99 = g[std::min(g.size() - 1, static_cast<size_t>(g.size() * 0.99))];
			// The display's interval: from the display itself when known,
			// else the short gaps
			const double refresh = DurandalGL::DisplayRefreshRate() > 0 ? 1000.0 / DurandalGL::DisplayRefreshRate() : g[g.size() / 20];
			// A hitch on screen: a gap of more than twice the typical frame
			// time (the refresh rate alone does not make a present late)
			std::vector<double> frame_dts;
			for (size_t i = 1; i < f.size(); ++i)
				frame_dts.push_back((f[i].time - f[i - 1].time) * 1000.0);
			std::sort(frame_dts.begin(), frame_dts.end());
			const double frame_median = frame_dts.empty() ? median : frame_dts[frame_dts.size() / 2];
			size_t hitches = 0;
			for (double d : gaps)
				if (d > frame_median * 2.0) ++hitches;
			double total = 0;
			for (double d : gaps) total += d;
			char buf[320];
			snprintf(buf, sizeof(buf),
				"  presented: %.1f fps, interval median %.2f ms, p99 %.2f ms, refresh %.2f ms (%.0f Hz), hitches (> 2x frame time) %zu of %zu (%.2f%%)\n",
				1000.0 * gaps.size() / total, median, p99, refresh, 1000.0 / refresh, hitches, gaps.size(), 100.0 * hitches / gaps.size());
			s << buf;
		}
	}

	std::string summary = s.str();
	if (FILE* out = fopen((path + ".summary.txt").c_str(), "w"))
	{
		fputs(summary.c_str(), out);
		fclose(out);
	}
	fputs(summary.c_str(), stdout);
	fflush(stdout);
	logNote("%s", summary.c_str());
}

}
