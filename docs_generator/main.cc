/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "markdown_gen.hh"
#include "schema_gen.hh"

#include <array>
#include <cxxopts.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

static const CompList generalComponents = {
    "name",
    "transform",
    "transform_snapshot",
    "event_bindings",
    "event_input",
    "scene_connection",
    "script",
    "signal_bindings",
    "signal_output",
    "audio",
};
static const CommonTypes generalCommonTypes = {
    typeid(ecs::EntityRef),
    typeid(ecs::SignalExpression),
};

static const CompList renderingComponents = {
    "renderable",
    "gui",
    "laser_line",
    "light_sensor",
    "light",
    "optic",
    "screen",
    "view",
    "voxel_area",
    "xr_view",
};
static const CommonTypes renderingCommonTypes = {};

static const CompList physicsComponents = {
    "physics",
    "physics_joints",
    "physics_query",
    "animation",
    "character_controller",
    "laser_emitter",
    "laser_sensor",
    "trigger_area",
    "trigger_group",
    "scene_properties",
};
static const CommonTypes physicsCommonTypes = {
    typeid(ecs::EntityRef),
    typeid(ecs::Transform),
};

static const CommonTypes prefabCommonTypes = {};
static const CommonTypes scriptsCommonTypes = {
    typeid(ecs::EntityRef),
    typeid(ecs::SignalExpression),
};

int main(int argc, char **argv) {
    cxxopts::Options options("docs_generator", "");
    options.positional_help("<output_dir>");
    options.add_options()("output-dir", "", cxxopts::value<std::string>());
    options.parse_positional({"output-dir"});

    auto optionsResult = options.parse(argc, argv);

    if (!optionsResult.count("output-dir")) {
        std::cout << options.help() << std::endl;
        return 1;
    }

    sp::logging::SetLogLevel(sp::logging::Level::Log);

    std::filesystem::path outputDir = optionsResult["output-dir"].as<std::string>();
    if (!std::filesystem::exists(outputDir)) {
        std::filesystem::create_directories(outputDir);
    }
    if (!std::filesystem::is_directory(outputDir)) {
        Errorf("Output path must be a directory: %s", outputDir.string());
        return 1;
    }

    std::vector<std::string> otherGroup;
    auto &nameComp = ecs::LookupComponent<ecs::Name>();
    otherGroup.emplace_back(nameComp.name);
    ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &) {
        otherGroup.emplace_back(name);
    });

    try {
        MarkdownContext(outputDir / "General_Components.md", PageType::Component)
            .SavePage(generalComponents, &generalCommonTypes, &otherGroup);
        MarkdownContext(outputDir / "Rendering_Components.md", PageType::Component)
            .SavePage(renderingComponents, &renderingCommonTypes, &otherGroup);
        MarkdownContext(outputDir / "Physics_Components.md", PageType::Component)
            .SavePage(physicsComponents, &physicsCommonTypes, &otherGroup);
        MarkdownContext(outputDir / "Other_Components.md", PageType::Component).SavePage(otherGroup);

        MarkdownContext(outputDir / "Prefab_Scripts.md", PageType::Prefab)
            .SavePage(ecs::GetScriptDefinitions().prefabs, &prefabCommonTypes);
        MarkdownContext(outputDir / "Runtime_Scripts.md", PageType::Script)
            .SavePage(ecs::GetScriptDefinitions().scripts, &scriptsCommonTypes);

        SchemaContext(outputDir / "scene.schema.json").SaveSchema();
    } catch (const std::exception &e) {
        Errorf("Markdown docs generation resulted in exception: %s", e.what());
        return 1;
    }
    return 0;
}
