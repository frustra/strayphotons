#if defined(_WIN32) && defined(PACKAGE_RELEASE)
	#include <windows.h>
#endif

#include <iostream>
using namespace std;

#include "Game.hh"
#include "Logging.hh"

#include <assets/AssetManager.hh>
#include <assets/Script.hh>
#include <cstdio>
#include <cxxopts.hpp>

#ifdef _WIN32
	#include <direct.h>
	#define os_getcwd _getcwd

// Instruct NVidia GPU to render this app on optimus-enabled machines (laptops with two GPUs)
extern "C" {
_declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}

#else
	#include <unistd.h>
	#define os_getcwd getcwd
#endif

//#define CATCH_GLOBAL_EXCEPTIONS

using cxxopts::value;

#if defined(_WIN32) && defined(PACKAGE_RELEASE)
	#define ARGC_NAME __argc
	#define ARGV_NAME __argv
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
#else
	#define ARGC_NAME argc
	#define ARGV_NAME argv
int main(int argc, char **argv)
#endif
{
	cxxopts::Options options("STRAYPHOTONS-TEST", "");

	options.positional_help("/path/to/script.txt]");
	options.add_options()("h,help", "Display help")("m,map", "Initial scene to load", value<string>())("basic-renderer",
		"Use minimal debug renderer",
		value<bool>())("size", "Initial window size", value<string>())("cvar",
		"Set cvar to initial value",
		value<vector<string>>())("script-file", "", value<string>());

	options.parse_positional({"script-file"});

#ifdef CATCH_GLOBAL_EXCEPTIONS
	try
#endif
	{
		auto optionsResult = options.parse(ARGC_NAME, ARGV_NAME);

		if (optionsResult.count("help")) {
			std::cout << options.help() << std::endl;
			return 0;
		}

		if (!optionsResult.count("script-file")) {
			Logf("Script file required argument.");
			return 0;
		}

		char cwd[FILENAME_MAX];
		os_getcwd(cwd, FILENAME_MAX);
		Logf("Starting in directory: %s", cwd);

		string scriptPath = optionsResult["script-file"].as<string>();
		auto script = sp::GAssets.LoadScript(scriptPath);
		if (!script) {
			Errorf("Script file not found: %s", scriptPath);
			return 0;
		}

		sp::Game game(optionsResult, script.get());
		return game.Start();
	}
#ifdef CATCH_GLOBAL_EXCEPTIONS
	catch (const char *err) {
		Errorf("terminating with exception: %s", err);
	} catch (const std::exception &ex) { Errorf("terminating with exception: %s", ex.what()); }
#endif
	return -1;
}
