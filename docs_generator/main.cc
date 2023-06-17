/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "field_docs.hh"
#include "schema_gen.hh"

#include <array>
#include <cxxopts.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

using CompList = std::vector<std::string>;
using CommonTypes = std::vector<std::type_index>;

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

inline std::string escapeMarkdownString(const std::string &input) {
    size_t escapeCount = std::count(input.begin(), input.end(), '|');
    if (escapeCount == 0) return input;
    std::string result;
    result.reserve(input.length() + escapeCount);
    for (auto &ch : input) {
        if (ch == '|') {
            result += '\\';
        }
        result += ch;
    }
    return result;
}

bool saveMarkdownComponentPage(const std::filesystem::path &outputPath,
    const CompList &compList,
    const CommonTypes *commonTypes = nullptr,
    CompList *otherList = nullptr) {
    std::set<std::string> savedDocs;
    std::set<std::string> seeAlso;

    std::ofstream file(outputPath);
    if (!file) {
        Errorf("Failed to open output file: '%s'", outputPath.string());
        return false;
    }

    std::function<void(const std::string &, const std::type_index &)> saveReferencedType;
    saveReferencedType = [&](const std::string &refName, const std::type_index &refType) {
        if (savedDocs.count(refName)) {
            seeAlso.emplace(refName);
            return;
        }
        savedDocs.emplace(refName);

        auto *metadata = ecs::StructMetadata::Get(refType);
        DocsStruct refDocs;
        bool isEnumFlags = false;
        bool isEnum = false;
        ecs::GetFieldType(refType, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;

            if constexpr (std::is_enum<T>()) {
                isEnum = true;
                if constexpr (is_flags_enum<T>()) {
                    isEnumFlags = true;
                }

                static const auto entries = magic_enum::enum_entries<T>();
                for (auto &[enumValue, enumName] : entries) {
                    const char *description = "No description";
                    if (metadata && metadata->enumMap) {
                        auto it = metadata->enumMap->find((uint32_t)enumValue);
                        if (it != metadata->enumMap->end()) {
                            description = it->second.c_str();
                        }
                    }
                    refDocs.fields.emplace_back(DocField{std::string(enumName), "", description, typeid(T), {}});
                }
            } else {
                static const T defaultComp = {};
                Assertf(metadata, "Unknown StructMetadata type %s", typeid(T).name());
                for (auto &field : metadata->fields) {
                    refDocs.AddField(field, field.Access(&defaultComp));
                }
            }
        });

        if (refDocs.fields.empty() && !metadata) {
            seeAlso.emplace(refName);
            return;
        }

        file << std::endl << "<div class=\"type_definition\">" << std::endl << std::endl;
        file << "### `" << refName << "` Type" << std::endl;
        if (isEnumFlags) {
            file << "This is an **enum flags** type. Multiple flags can be combined using the `|` "
                    "character (e.g. `\"One|Two\"` with no whitespace). Flag names are case-sensitive.  ";
            file << std::endl << "Enum flag names:" << std::endl;
        } else if (isEnum) {
            file << "This is an **enum** type, and can be one of the following case-sensitive values:" << std::endl;
        }
        if (isEnum) {
            for (auto &field : refDocs.fields) {
                file << "- \"**" << field.name << "**\" - " << field.description << "" << std::endl;
            }
        } else if (!refDocs.fields.empty()) {
            file << "| Field Name | Type | Default Value | Description |" << std::endl;
            file << "|------------|------|---------------|-------------|" << std::endl;
            for (auto &field : refDocs.fields) {
                file << "| **" << field.name << "** | " << field.typeString << " | "
                     << escapeMarkdownString(field.defaultValue.serialize()) << " | " << field.description << " |"
                     << std::endl;
            }
        }

        if (metadata && !metadata->description.empty()) {
            file << metadata->description << std::endl;
        }

        file << std::endl << "</div>" << std::endl;

        for (auto &[subRefName, subRefType] : refDocs.references) {
            if (!savedDocs.count(subRefName)) {
                saveReferencedType(subRefName, subRefType);
            } else {
                seeAlso.emplace(subRefName);
            }
        }
    };

    if (commonTypes && !commonTypes->empty()) {
        file << std::endl << "<div class=\"component_definition\">" << std::endl << std::endl;
        file << "## Common Types" << std::endl;
        for (auto &type : *commonTypes) {
            auto *metadata = ecs::StructMetadata::Get(type);
            Assertf(metadata, "Type has no metadata: %s", type.name());
            saveReferencedType(metadata->name, metadata->type);
        }
        file << std::endl << "</div>" << std::endl << std::endl;
    }

    for (auto &name : compList) {
        file << std::endl << "<div class=\"component_definition\">" << std::endl << std::endl;
        file << "## `" << name << "` Component" << std::endl;

        if (otherList) sp::erase(*otherList, name);

        auto &comp = *ecs::LookupComponent(name);

        DocsStruct docs;
        ecs::GetFieldType(comp.metadata.type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;
            static const T defaultComp = {};
            for (auto &field : comp.metadata.fields) {
                docs.AddField(field, field.Access(&defaultComp));
            }
        });

        if (docs.fields.empty()) {
            if (!comp.metadata.description.empty()) {
                file << comp.metadata.description << std::endl;
            } else {
                file << "The `" << name << "` component has no public fields" << std::endl;
            }
        } else if (docs.fields.size() == 1 && docs.fields.front().name.empty()) {
            file << "The `" << name << "` component has type: " << docs.fields.front().typeString << std::endl;

            if (!comp.metadata.description.empty()) {
                file << comp.metadata.description << std::endl;
            }
        } else {
            file << "| Field Name | Type | Default Value | Description |" << std::endl;
            file << "|------------|------|---------------|-------------|" << std::endl;
            for (auto &field : docs.fields) {
                file << "| **" << field.name << "** | " << field.typeString << " | "
                     << escapeMarkdownString(field.defaultValue.serialize()) << " | " << field.description << " |"
                     << std::endl;
            }

            if (!comp.metadata.description.empty()) {
                file << comp.metadata.description << std::endl;
            }
        }

        if (!docs.references.empty()) {
            seeAlso.clear();

            for (auto &[refName, refType] : docs.references) {
                saveReferencedType(refName, refType);
            }

            if (!seeAlso.empty()) {
                file << std::endl << "**See Also:**" << std::endl;
                for (auto &refName : seeAlso) {
                    file << "[" << refName << "](#" << refName << "-type)" << std::endl;
                }
            }
        }

        file << std::endl << "</div>" << std::endl << std::endl;
    }

    return true;
}

bool saveMarkdownScriptPage(const std::filesystem::path &outputPath,
    const std::string &type,
    const std::map<std::string, ecs::ScriptDefinition> &scriptDefinitions) {
    std::ofstream file(outputPath);
    if (!file) {
        Errorf("Failed to open output file: '%s'", outputPath.string());
        return false;
    }

    for (auto &[name, def] : scriptDefinitions) {
        if (!def.context) continue;

        file << std::endl << "<div class=\"script_definition\">" << std::endl << std::endl;
        file << "## `" << name << "` " << type << std::endl;

        auto &metadata = def.context->metadata;
        const void *defaultScript = def.context->GetDefault();

        DocsStruct docs;
        for (auto &field : metadata.fields) {
            docs.AddField(field, field.Access(defaultScript));
        }

        if (docs.fields.empty()) {
            if (!metadata.description.empty()) {
                file << metadata.description << std::endl;
            } else {
                file << "The `" << name << "` " << sp::to_lower_copy(type) << " has no configurable parameters"
                     << std::endl;
            }
        } else if (docs.fields.size() == 1 && docs.fields.front().name.empty()) {
            if (!metadata.description.empty()) {
                file << metadata.description << std::endl << std::endl;
            }

            file << "The `" << name << "` " << sp::to_lower_copy(type)
                 << " has parameter type: " << docs.fields.front().typeString << std::endl;
        } else {
            if (!metadata.description.empty()) {
                file << metadata.description << std::endl << std::endl;
            }

            file << "| Parameter Name | Type | Default Value | Description |" << std::endl;
            file << "|----------------|------|---------------|-------------|" << std::endl;
            for (auto &field : docs.fields) {
                file << "| **" << field.name << "** | " << field.typeString << " | "
                     << escapeMarkdownString(field.defaultValue.serialize()) << " | " << field.description << " |"
                     << std::endl;
            }
        }

        file << std::endl << "</div>" << std::endl;
    }

    return true;
}

template<typename... AllComponentTypes, template<typename...> typename ECSType, typename Func>
void forEachComponentType(ECSType<AllComponentTypes...> *, Func &&callback) {
    ( // For each component:
        [&] {
            using T = AllComponentTypes;

            if constexpr (std::is_same_v<T, ecs::Name> || std::is_same_v<T, ecs::SceneInfo>) {
                // Skip
            } else if constexpr (!Tecs::is_global_component<T>()) {
                callback((T *)nullptr);
            }
        }(),
        ...);
}

// Calls the provided auto-lambda for all components except Name and SceneInfo
template<typename Func>
void ForEachComponentType(Func &&callback) {
    forEachComponentType((ecs::ECS *)nullptr, std::forward<Func>(callback));
}

bool saveJsonSchema(const std::filesystem::path &outputPath) {
    SchemaContext ctx;
    picojson::object entityProperties;

    std::ofstream file(outputPath);
    if (!file) {
        Errorf("Failed to open output file: '%s'", outputPath.string());
        return false;
    }

    auto &nameComp = ecs::LookupComponent<ecs::Name>();
    sp::json::SaveSchema<ecs::Name>(entityProperties[nameComp.name]);
    ForEachComponentType([&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;

        auto &comp = ecs::LookupComponent<T>();
        sp::json::SchemaTypeReferences references;
        sp::json::SaveSchema<T>(entityProperties[comp.name], &references);

        for (auto *metadata : references) {
            if (metadata) ctx.AddDefinition(*metadata);
        }
    });

    picojson::object root;
    root["$schema"] = picojson::value("http://json-schema.org/draft-07/schema#");
    root["title"] = picojson::value("Scene Definition");
    root["type"] = picojson::value("object");
    if (!ctx.definitions.empty()) root["definitions"] = picojson::value(ctx.definitions);

    picojson::object entityItems;
    entityItems["type"] = picojson::value("object");
    entityItems["properties"] = picojson::value(entityProperties);

    picojson::object entityProperty;
    entityProperty["type"] = picojson::value("array");
    entityProperty["items"] = picojson::value(entityItems);

    picojson::object properties;
    properties["entities"] = picojson::value(entityProperty);

    root["properties"] = picojson::value(properties);
    file << picojson::value(root).serialize(true);

    return true;
}

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

    if (!saveMarkdownComponentPage(outputDir / "General_Components.md",
            generalComponents,
            &generalCommonTypes,
            &otherGroup)) {
        return 1;
    }
    if (!saveMarkdownComponentPage(outputDir / "Rendering_Components.md",
            renderingComponents,
            &renderingCommonTypes,
            &otherGroup)) {
        return 1;
    }
    if (!saveMarkdownComponentPage(outputDir / "Physics_Components.md",
            physicsComponents,
            &physicsCommonTypes,
            &otherGroup)) {
        return 1;
    }
    if (!saveMarkdownComponentPage(outputDir / "Other_Components.md", otherGroup)) return 1;

    if (!saveMarkdownScriptPage(outputDir / "Prefab_Scripts.md", "Prefab", ecs::GetScriptDefinitions().prefabs)) {
        return 1;
    }
    if (!saveMarkdownScriptPage(outputDir / "Runtime_Scripts.md", "Script", ecs::GetScriptDefinitions().scripts)) {
        return 1;
    }

    if (!saveJsonSchema(outputDir / "scene.schema.json")) return 1;
    return 0;
}
