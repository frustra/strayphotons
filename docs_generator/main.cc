#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "field_docs.hh"

#include <array>
#include <cxxopts.hpp>
#include <fstream>
#include <sstream>

static const std::vector<std::string> generalComponents = {
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

static const std::vector<std::string> renderingComponents = {
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

static const std::vector<std::string> physicsComponents = {
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

static const std::map<std::string, const std::vector<std::string> *> docGroups = {
    {"general", &generalComponents},
    {"rendering", &renderingComponents},
    {"physics", &physicsComponents},
    {"other", nullptr},
    {"schema", nullptr},
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

void saveMarkdownPage(std::ofstream &file, const std::vector<std::string> &compList) {
    std::set<std::string> savedDocs;
    for (auto &name : compList) {
        file << "## `" << name << "` Component" << std::endl;

        auto &comp = *ecs::LookupComponent(name);

        DocsContext docs;
        ecs::GetFieldType(comp.metadata.type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;
            docs.SaveFields<T>(true);
        });

        if (docs.fields.empty()) {
            file << "The `" << name << "` component has no public fields" << std::endl;
        } else if (docs.fields.size() == 1 && docs.fields.front().name.empty()) {
            file << "The `" << name << "` component has type: " << docs.fields.front().typeString << std::endl;
        } else {
            file << "| Field Name | Type | Default Value | Description |" << std::endl;
            file << "|------------|------|---------------|-------------|" << std::endl;
            for (auto &field : docs.fields) {
                file << "| **" << field.name << "** | " << field.typeString << " | "
                     << escapeMarkdownString(field.defaultValue.serialize()) << " | " << field.description << " |"
                     << std::endl;
            }
        }

        if (!docs.references.empty()) {
            std::set<std::string> seeAlso;

            std::function<void(const std::string &, const std::type_index &)> saveReferencedType;
            saveReferencedType = [&](const std::string &refName, const std::type_index &refType) {
                savedDocs.emplace(refName);

                DocsContext refDocs;
                bool isEnumFlags = false;
                bool isEnum = false;
                ecs::GetFieldType(refType, [&](auto *typePtr) {
                    using T = std::remove_pointer_t<decltype(typePtr)>;

                    if constexpr (std::is_enum<T>()) {
                        isEnum = true;
                        if constexpr (is_flags_enum<T>()) {
                            isEnumFlags = true;
                        }

                        static const auto names = magic_enum::enum_names<T>();
                        for (auto &enumName : names) {
                            refDocs.fields.emplace_back(DocField{std::string(enumName), "", "", typeid(T), {}});
                        }
                    } else {
                        refDocs.SaveFields<T>(true);
                    }
                });

                if (refDocs.fields.empty()) {
                    seeAlso.emplace(refName);
                    return;
                }

                file << std::endl << "### `" << refName << "` Type" << std::endl;
                if (isEnumFlags) {
                    file << "This is a flags enum type. Multiple flags can be combined using the '|' character "
                         << "(e.g. `\"One|Two\"` with no whitespace)." << std::endl;
                }
                if (isEnum) {
                    file << "Note: Enum string names are case-sensitive." << std::endl;
                    file << "| Enum Value |" << std::endl;
                    file << "|------------|" << std::endl;
                    for (auto &field : refDocs.fields) {
                        file << "| \"" << field.name << "\" |" << std::endl;
                    }
                } else {
                    file << "| Field Name | Type | Default Value | Description |" << std::endl;
                    file << "|------------|------|---------------|-------------|" << std::endl;
                    for (auto &field : refDocs.fields) {
                        file << "| **" << field.name << "** | " << field.typeString << " | "
                             << escapeMarkdownString(field.defaultValue.serialize()) << " | " << field.description
                             << " |" << std::endl;
                    }
                }

                for (auto &[subRefName, subRefType] : refDocs.references) {
                    if (!savedDocs.count(subRefName)) {
                        saveReferencedType(subRefName, subRefType);
                    } else {
                        seeAlso.emplace(subRefName);
                    }
                }
            };

            for (auto &[refName, refType] : docs.references) {
                saveReferencedType(refName, refType);
            }

            if (!seeAlso.empty()) {
                file << std::endl << "**See Also:**" << std::endl;
                for (auto &refName : seeAlso) {
                    file << "`" << refName << "`" << std::endl;
                }
            }
        }

        file << std::endl << std::endl;
    }
}

void saveJsonSchema(std::ofstream &file) {
    picojson::object extraDefinitions;
    picojson::object entityProperties;

    std::function<void(const ecs::StructMetadata &)> saveReferencedType;
    saveReferencedType = [&](const ecs::StructMetadata &metadata) {
        if (extraDefinitions.count(metadata.name)) return;

        ecs::GetFieldType(metadata.type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;

            sp::json::SchemaTypeReferences references;
            picojson::value typeSchema;
            sp::json::SaveSchema<T>(typeSchema, &references);
            extraDefinitions.emplace(metadata.name, typeSchema.get<picojson::object>());

            for (auto *ref : references) {
                if (ref) saveReferencedType(*ref);
            }
        });
    };

    auto addComponent = [&](auto &name, auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;

        sp::json::SchemaTypeReferences references;
        sp::json::SaveSchema<T>(entityProperties[name], &references);

        for (auto *metadata : references) {
            if (metadata) saveReferencedType(*metadata);
        }
    };

    auto &nameComp = ecs::LookupComponent<ecs::Name>();
    addComponent(nameComp.name, (ecs::Name *)nullptr);
    ecs::ForEachComponent([&](auto &name, auto &base) {
        ecs::GetFieldType(base.metadata.type, [&](auto *typePtr) {
            addComponent(name, typePtr);
        });
    });

    picojson::object root;
    root["$schema"] = picojson::value("http://json-schema.org/draft-07/schema#");
    root["title"] = picojson::value("Scene Definition");
    root["type"] = picojson::value("object");
    if (!extraDefinitions.empty()) {
        root["definitions"] = picojson::value(extraDefinitions);
    }

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
}

int main(int argc, char **argv) {
    cxxopts::Options options("docs_generator", "");
    options.positional_help("<doc-group> <output_path>");
    options.add_options()("doc-group", "", cxxopts::value<std::string>());
    options.add_options()("output-path", "", cxxopts::value<std::string>());
    options.parse_positional({"doc-group", "output-path"});

    auto optionsResult = options.parse(argc, argv);

    if (!optionsResult.count("doc-group") || !optionsResult.count("output-path")) {
        std::cout << options.help() << std::endl;
        return 1;
    }

    sp::logging::SetLogLevel(sp::logging::Level::Log);

    std::string docGroup = optionsResult["doc-group"].as<std::string>();
    std::string outputPath = optionsResult["output-path"].as<std::string>();

    if (!docGroups.count(docGroup)) {
        std::cout << "Unknown documentation group: " << docGroup << std::endl;
        std::cout << "Options are:";
        for (auto &group : docGroups) {
            std::cout << " " << group.first;
        }
        std::cout << std::endl;
        return 1;
    }

    std::vector<std::string> otherGroup;
    auto &groupFilterPtr = docGroups.at(docGroup);
    auto &groupFilter = groupFilterPtr ? *groupFilterPtr : otherGroup;

    if (groupFilterPtr == nullptr && docGroup != "schema") {
        // Special case for the "other" group which documents all unlisted components.
        std::vector<std::string> allListedComponents;
        for (auto &group : docGroups) {
            if (group.second) {
                allListedComponents.insert(allListedComponents.end(), group.second->begin(), group.second->end());
            }
        }

        auto &nameComp = ecs::LookupComponent<ecs::Name>();
        if (!sp::contains(allListedComponents, nameComp.name)) otherGroup.emplace_back(nameComp.name);
        ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &) {
            if (!sp::contains(allListedComponents, name)) otherGroup.emplace_back(name);
        });
    }

    std::ofstream file(outputPath);
    if (!file) {
        Errorf("Failed to open output file: '%s'", outputPath);
        return 1;
    }

    if (docGroup == "schema") {
        saveJsonSchema(file);
    } else {
        saveMarkdownPage(file, groupFilter);
    }
    return 0;
}
