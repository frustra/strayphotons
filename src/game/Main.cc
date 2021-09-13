#ifdef _WIN32
    #include <windows.h>
#endif

#include <iostream>
using namespace std;

#include "Game.hh"
#include "core/Logging.hh"

#ifdef SP_TEST_MODE
    #include "assets/AssetManager.hh"
    #include "assets/Script.hh"
#endif

#include <cstdio>
#include <cxxopts.hpp>
#include <filesystem>
#include <memory>

//#define CATCH_GLOBAL_EXCEPTIONS

using cxxopts::value;

#if defined(_WIN32) && defined(SP_PACKAGE_RELEASE)
    #define ARGC_NAME __argc
    #define ARGV_NAME __argv
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
#else
    #define ARGC_NAME argc
    #define ARGV_NAME argv
int main(int argc, char **argv)
#endif
{
#ifdef SP_TEST_MODE
    cxxopts::Options options("STRAYPHOTONS-TEST", "");
    options.positional_help("/path/to/script.txt]");
#else
    cxxopts::Options options("STRAYPHOTONS", "");
#endif

    // clang-format off
    options.add_options()
        ("h,help", "Display help")
        ("m,map", "Initial scene to load", value<string>())
        ("size", "Initial window size", value<string>())
#ifdef SP_TEST_MODE
        ("script-file", "", value<string>())
#endif
        ("cvar", "Set cvar to initial value", value<vector<string>>());
    // clang-format on

#ifdef SP_TEST_MODE
    options.parse_positional({"script-file"});
#endif

#ifdef _WIN32
    // Increase thread scheduler resolution from default of 15ms
    timeBeginPeriod(1);
    std::shared_ptr<UINT> timePeriodReset(new UINT(1), [](UINT *period) {
        timeEndPeriod(*period);
        delete period;
    });
#endif

#ifdef CATCH_GLOBAL_EXCEPTIONS
    try
#endif
    {
        auto optionsResult = options.parse(ARGC_NAME, ARGV_NAME);

        if (optionsResult.count("help")) {
            std::cout << options.help() << std::endl;
            return 0;
        }

#ifdef SP_TEST_MODE
        if (!optionsResult.count("script-file")) {
            Logf("Script file required argument.");
            return 0;
        }
#endif

        Logf("Starting in directory: %s", std::filesystem::current_path().string());

#ifdef SP_TEST_MODE
        string scriptPath = optionsResult["script-file"].as<string>();
        auto script = sp::GAssets.LoadScript(scriptPath);
        if (!script) {
            Errorf("Script file not found: %s", scriptPath);
            return 0;
        }

        sp::Game game(optionsResult, script.get());
        return game.Start();
#else
        sp::Game game(optionsResult);
        return game.Start();
#endif
    }
#ifdef CATCH_GLOBAL_EXCEPTIONS
    catch (const char *err) {
        Errorf("terminating with exception: %s", err);
    } catch (const std::exception &ex) { Errorf("terminating with exception: %s", ex.what()); }
#endif
    return -1;
}
