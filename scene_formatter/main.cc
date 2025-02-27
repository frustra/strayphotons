/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Logging.hh"
#include "game/SceneManager.hh"

#include <assets/AssetManager.hh>
#include <cxxopts.hpp>
#include <filesystem>
#include <fstream>

int main(int argc, char **argv) {
    cxxopts::Options options("scene_formatter", "");
    options.positional_help("<scene_name>");
    // clang-format off
    options.add_options()
        ("assets", "Override path to assets folder", cxxopts::value<std::string>())
        ("scene-name", "", cxxopts::value<std::string>());
    // clang-format on
    options.parse_positional({"scene-name"});

    auto optionsResult = options.parse(argc, argv);

    if (!optionsResult.count("scene-name")) {
        std::cout << options.help() << std::endl;
        return 1;
    }

    std::string sceneName = optionsResult["scene-name"].as<std::string>();

    sp::logging::SetLogLevel(sp::logging::Level::Log);

    std::string assetsPath = "";
    if (optionsResult.count("assets")) assetsPath = optionsResult["assets"].as<std::string>();
    sp::Assets().StartThread(assetsPath);

    sp::SceneManager scenes;
    scenes.DisableGraphicsPreload();
    scenes.DisablePhysicsPreload();
    scenes.QueueActionAndBlock(sp::SceneAction::LoadScene, sceneName);
    scenes.QueueActionAndBlock(sp::SceneAction::SaveStagingScene, sceneName);
    return 0;
}
