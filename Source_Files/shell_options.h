#ifndef SHELL_OPTIONS_H
#define SHELL_OPTIONS_H

#include <string>
#include <vector>
#include <unordered_map>

struct ShellOptions {
	std::unordered_map<int, bool> parse(int argc, char** argv, bool ignore_unknown_args = false);

	std::string program_name;
	
	bool nogl;
	bool nosound;
	bool nogamma;
	bool debug;
	bool nojoystick;
	bool insecure_lua;

	bool force_fullscreen;
	bool force_windowed;

	bool skip_intro;
	bool editor;

	bool no_chooser;

	std::string replay_directory;

	// Durandal: frame-time benchmark (see Misc/DurandalBenchmark.h)
	std::string benchmark_log;
	std::string benchmark_size;
	std::string benchmark_fps;
	std::string benchmark_shots;		// directory for OpenGL/Metal parity shots
	std::string benchmark_shot_every;	// ticks between shots (default 600)
	std::string benchmark_speed;		// film replay speed (default real time)
	bool benchmark_hidden = false;		// never show the window (development)

	std::string directory;
	std::vector<std::string> files;

	std::string output;
};

extern ShellOptions shell_options;

#endif
