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

using cxxopts::value;

int main(int argc, char **argv)
{
	char cwd[FILENAME_MAX];
	os_getcwd(cwd, FILENAME_MAX);
	Logf("Starting in directory: %s", cwd);

	cxxopts::Options options("STRAYPHOTONS", "");

	options.add_options()
	("basic-renderer", "Use minimal debug renderer", value<bool>())
	("map", "Boot into scene", value<string>()->default_value("test1"))
	;

	try
	{
		options.parse(argc, argv);

		sp::Game game(options);
		game.Start();
		return 0;
	}
	catch (const char *err)
	{
		Errorf("terminating with exception: %s", err);
	}
	catch (const std::exception &ex)
	{
		Errorf("terminating with exception: %s", ex.what());
	}
	return -1;
}

