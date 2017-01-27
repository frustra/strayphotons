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

int main(int argc, char **argv)
{
	cxxopts::Options options("STRAYPHOTONS", "");

	options.add_options()
	("h,help", "Display help")
	("m,map", "Boot into scene", value<string>()->default_value("test1"))
	("basic-renderer", "Use minimal debug renderer", value<bool>())
	("voxel-gridsize", "Number of voxels (NxNxN)", value<int>()->default_value("256"))
	("voxel-supersample", "Resolution multiplier for generating voxel grid", value<float>()->default_value("1.0"))
	("cvar", "Set cvar to initial value", value<vector<string>>())
	("audio-driver", "Use audio device by index", value<int>())
	("audio-output-type", "Force use of audio API (pulseaudio, alsa)", value<string>())
	;

#ifdef CATCH_GLOBAL_EXCEPTIONS
	try
#endif
	{
		options.parse(argc, argv);

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

