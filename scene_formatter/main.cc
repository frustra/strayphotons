#include "core/Logging.hh"
#include "game/SceneManager.hh"

#include <cxxopts.hpp>
#include <filesystem>
#include <fstream>

int main(int argc, char **argv) {
    cxxopts::Options options("scene_formatter", "");
    options.positional_help("<scene_name>");
    options.add_options()("scene-name", "", cxxopts::value<std::string>());
    options.parse_positional({"scene-name"});

    auto optionsResult = options.parse(argc, argv);

    if (!optionsResult.count("scene-name")) {
        std::cout << options.help() << std::endl;
        return 1;
    }

    std::string sceneName = optionsResult["scene-name"].as<std::string>();

    sp::logging::SetLogLevel(sp::logging::Level::Log);

    sp::SceneManager scenes(true);
    scenes.QueueActionAndBlock(sp::SceneAction::LoadScene, sceneName);
    scenes.QueueActionAndBlock(sp::SceneAction::SaveStagingScene, sceneName);
    return 0;
}
