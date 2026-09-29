#include "shell_options.h"
#include <cstdlib>

#include <iostream>
#include <functional>
#include <sstream>

#include "FileHandler.h"
#include "Logging.h"
#include "csstrings.h"

#ifdef __WIN32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static void print_usage();
static void print_version();

ShellOptions shell_options;

struct ShellOptionsOption {
	bool match(const std::string& s) {
		if (s[0] == '-') {
			if (s[1] == '-') {
				return long_name == s.substr(2);
			} else if (short_name.size()) {
				return short_name == s.substr(1);
			}
		}

		return false;
	}
	
	std::string short_name;
	std::string long_name;
	std::string help;
};

static int help_tab_stop = 33;

static std::string spaces(int num_spaces)
{
	std::string s;
	for (auto i = 0; i < num_spaces; ++i) {
		s += " ";
	}
	return s;
}

static std::ostream& operator<<(std::ostream& s, const ShellOptionsOption& o) {
	if (o.help.size())
	{
		auto num_spaces = help_tab_stop - 12 - o.long_name.size();
		
		s << "\t[";
		if (o.short_name.size())
		{
			num_spaces -= 4;
			num_spaces -= o.short_name.size();
			s << "-" << o.short_name << " | ";
		}
		
		s << "--" << o.long_name << "]" << spaces(num_spaces) << o.help << "\n";
	}

	return s;
}

struct ShellOptionsCommand : public ShellOptionsOption {
	std::function<void()> command;
};

struct ShellOptionsFlag : public ShellOptionsOption {
	bool& flag;
};

struct ShellOptionsString : public ShellOptionsOption {
	std::string& string;
};

static std::string ignore;

static const std::vector<ShellOptionsCommand> shell_options_commands {
	{"h", "help", "Display this help message", print_usage},
	{"v", "version", "Display the game version", print_version}
};

static const std::vector<ShellOptionsFlag> shell_options_flags {
	{"d", "debug", "Allow saving of core files", shell_options.debug},
	{"f", "fullscreen", "Run the game fullscreen", shell_options.force_fullscreen},
	{"w", "windowed", "Run the game in a window", shell_options.force_windowed},
	{"g", "nogl", "Do not use OpenGL", shell_options.nogl},
	{"s", "nosound", "Do not access the sound card", shell_options.nosound},
	{"m", "nogamma", "Disable gamma table effects (menu fades)", shell_options.nogamma},
	{"j", "nojoystick", "Do not initialize joysticks", shell_options.nojoystick},
	{"i", "insecure_lua", "", shell_options.insecure_lua},
	{"Q", "skip-intro", "Skip intro screens", shell_options.skip_intro},
	{"e", "editor", "Use editor prefs; jump directly to map", shell_options.editor},
	{"", "no-chooser", "Disable the scenario chooser", shell_options.no_chooser},
	{"", "benchmark-hidden", "With --benchmark: never show the window", shell_options.benchmark_hidden}
};

static const std::vector<ShellOptionsString> shell_options_strings {
	{"o", "output", "With -e, output to [file] and exit on quit", shell_options.output},
	{"l", "replay-directory", "Directory with replays to load", shell_options.replay_directory},
	{"", "benchmark", "Play the given film, log frame times to [file], quit", shell_options.benchmark_log},
	{"", "benchmark-size", "With --benchmark: [WxH] window or native", shell_options.benchmark_size},
	{"", "benchmark-fps", "With --benchmark: frame rate target, 0 = uncapped", shell_options.benchmark_fps},
	{"", "benchmark-shots", "With --benchmark: save OpenGL/Metal parity shots to [dir]", shell_options.benchmark_shots},
	{"", "benchmark-shot-every", "With --benchmark-shots: ticks between shots", shell_options.benchmark_shot_every},
	{"", "benchmark-speed", "With --benchmark: film replay speed", shell_options.benchmark_speed},
	{"NSDocumentRevisionsDebugMode", "", "", ignore} // annoying Xcode argument
};

std::unordered_map<int, bool> ShellOptions::parse(int argc, char** argv, bool ignore_unknown_args)
{
	shell_options.program_name = argv[0];
	--argc;
	++argv;

    std::vector<std::string> args;
    while (argc > 0)
    {
        if (strncmp(*argv, "-C", 2) == 0)
        {
            args.push_back(*argv + 2);
        }
        else
        {
            args.push_back(*argv);
        }

        --argc;
        ++argv;
    }

	std::unordered_map<int, bool> results;

    for (int i = 0; i < args.size(); i++)
    {
		const auto& arg = args[i];
		bool found = false;

		for (auto command : shell_options_commands)
		{
			if (command.match(arg))
			{
				command.command();
				exit(0);
			}
		}

		for (auto flag : shell_options_flags)
		{
			if (flag.match(arg))
			{
				found = true;
				flag.flag = true;
				break;
			}
		}

		for (auto option : shell_options_strings)
		{
			if (option.match(arg))
			{
				if (i < args.size() - 1 && args[i + 1][0] != '-')
				{
					found = true;
					results.insert({ i++ + 1, true });
                    option.string = args[i];
                }
                else
                {
					logFatal("%s requires an additional argument", arg.c_str());
                    printf("%s requires an additional argument\n", arg.c_str());
                    print_usage();
                    exit(1);
                }
			}
		}

		if (!found)
		{
			if (arg[0] != '-')
			{
				FileSpecifier f(arg);
				if (f.Exists())
				{
					if (f.IsDir())
					{
						shell_options.directory = arg;
					}
					else
					{
						shell_options.files.push_back(arg);
					}

					found = true;
				}
			}

			if (!found && !ignore_unknown_args)
			{
				logFatal("Unrecognized argument '%s'.", arg.c_str());
				printf("Unrecognized argument '%s'.\n", arg.c_str());
				print_usage();
				exit(1);
			}
		}

		results.insert({ i + 1, found });
	}

	// Durandal: benchmark settings may come from the environment instead,
	// so nothing but dash flags appears on the command line. macOS treats
	// other command-line words as documents to open and shows an error
	// alert when it can't, which would appear on screen in an unattended run.
	if (const char* v = getenv("DURANDAL_BENCHMARK"))
		shell_options.benchmark_log = v;
	if (const char* v = getenv("DURANDAL_BENCHMARK_FILM"))
		shell_options.files.push_back(v);
	if (const char* v = getenv("DURANDAL_BENCHMARK_SHOTS"))
		shell_options.benchmark_shots = v;
	if (const char* v = getenv("DURANDAL_BENCHMARK_SIZE"))
		shell_options.benchmark_size = v;
	if (const char* v = getenv("DURANDAL_BENCHMARK_FPS"))
		shell_options.benchmark_fps = v;
	if (const char* v = getenv("DURANDAL_BENCHMARK_SPEED"))
		shell_options.benchmark_speed = v;
	if (const char* v = getenv("DURANDAL_BENCHMARK_SHOT_EVERY"))
		shell_options.benchmark_shot_every = v;
	if (const char* v = getenv("DURANDAL_BENCHMARK_HIDDEN"))
		shell_options.benchmark_hidden = std::string(v) == "1";

	return results;
}

void print_version()
{
	char app_name_version[256];
	expand_app_variables(app_name_version, "Aleph One $appLongVersion$");
	std::cout << app_name_version << std::endl;
}

void print_usage()
{
	std::ostringstream oss;

#ifdef __WIN32__
	oss << "Command line switches:\n\n";
#else
	oss << "\nUsage: " << shell_options.program_name << " [options] [directory] [file]\n";
#endif

	for (auto command : shell_options_commands)
	{
		oss << command;
	}

	for (auto flag : shell_options_flags)
	{
		oss << flag;
	}

	for (auto option : shell_options_strings)
	{
		oss << option;
	}

	oss << "\tdirectory" << spaces(help_tab_stop - strlen("directory") - 8)
		<< "Directory containing scenario data files\n"
		
		<< "\tfile" << spaces(help_tab_stop - strlen("file") - 8)
		<< "Saved game to load or film to play\n"
		<< "\n"
		<< "You can also use the ALEPHONE_DATA environment variable to specify\n"
		<< "the data directory.\n";

#ifdef __WIN32__
	MessageBoxW(NULL, utf8_to_wide(oss.str()).c_str(), L"Usage", MB_OK | MB_ICONINFORMATION);
#else
	std::cout << oss.str();
#endif
}
