#include <iostream>
using namespace std;

#include "core/Game.hh"
#include "core/Logging.hh"

#include <cxxopts.hpp>
#include <cstdio>

#ifdef _WIN32
#include <direct.h>
#define os_getcwd _getcwd
#else
#include <unistd.h>
#define os_getcwd getcwd
#endif

//#define CATCH_GLOBAL_EXCEPTIONS

using cxxopts::value;

#if defined(_WIN32) && defined(PACKAGE_RELEASE)
#include <windows.h>
#define ARGC_NAME __argc
#define ARGV_NAME __argv
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
#else
#define ARGC_NAME argc
#define ARGV_NAME argv
int main(int argc, char **argv)
#endif
{
	cxxopts::Options options("STRAYPHOTONS", "");

	options.add_options()
	("h,help", "Display help")
	("m,map", "Initial scene to load", value<string>())
	("basic-renderer", "Use minimal debug renderer", value<bool>())
	("cvar", "Set cvar to initial value", value<vector<string>>())
	("audio-driver", "Use audio device by index", value<int>())
	("audio-output-type", "Force use of audio API (pulseaudio, alsa)", value<string>())
	;

#ifdef CATCH_GLOBAL_EXCEPTIONS
	try
#endif
	{
		options.parse(ARGC_NAME, ARGV_NAME);

		if (options.count("help"))
		{
			std::cout << options.help() << std::endl;
			return 0;
		}

		char cwd[FILENAME_MAX];
		os_getcwd(cwd, FILENAME_MAX);
		Logf("Starting in directory: %s", cwd);

		sp::Game game(options);
		game.Start();
		return 0;
	}
#ifdef CATCH_GLOBAL_EXCEPTIONS
	catch (const char *err)
	{
		Errorf("terminating with exception: %s", err);
	}
	catch (const std::exception &ex)
	{
		Errorf("terminating with exception: %s", ex.what());
	}
#endif
	return -1;
}

