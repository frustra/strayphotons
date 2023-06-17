/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "assets/JsonHelpers.hh"
#include "ecs/Ecs.hh"
#include "ecs/StructFieldTypes.hh"

#include <filesystem>
#include <fstream>
#include <sstream>

using CompList = std::vector<std::string>;
using CommonTypes = std::vector<std::type_index>;

struct DocField {
    std::string name, typeString, description;
    std::type_index type;
    picojson::value defaultValue;
};

struct DocsStruct {
    std::vector<DocField> fields;
    std::map<std::string, std::type_index> references;

private:
    template<typename T>
    std::string fieldTypeName() {
        if constexpr (std::is_same_v<T, bool>) {
            return "bool";
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return "int32";
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return "uint32";
        } else if constexpr (std::is_same_v<T, size_t>) {
            return "size_t";
        } else if constexpr (std::is_same_v<T, sp::angle_t>) {
            return "float (degrees)";
        } else if constexpr (std::is_same_v<T, float>) {
            return "float";
        } else if constexpr (std::is_same_v<T, double>) {
            return "double";
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "string";
        } else if constexpr (std::is_same_v<T, sp::color_t>) {
            return "vec3 (red, green, blue)";
        } else if constexpr (std::is_same_v<T, sp::color_alpha_t>) {
            return "vec4 (red, green, blue, alpha)";
        } else if constexpr (std::is_same_v<T, glm::quat> || std::is_same_v<T, glm::mat3>) {
            return "vec4 (angle_degrees, axis_x, axis_y, axis_z)";
        } else if constexpr (sp::is_glm_vec<T>()) {
            using U = typename T::value_type;

            if constexpr (std::is_same_v<U, float>) {
                return "vec" + std::to_string(T::length());
            } else if constexpr (std::is_same_v<U, double>) {
                return "dvec" + std::to_string(T::length());
            } else if constexpr (std::is_same_v<U, int>) {
                return "ivec" + std::to_string(T::length());
            } else if constexpr (std::is_same_v<U, unsigned int>) {
                return "uvec" + std::to_string(T::length());
            } else {
                return typeid(T).name();
            }
        } else if constexpr (sp::is_vector<T>()) {
            return "vector&lt;" + fieldTypeName<typename T::value_type>() + "&gt;";
        } else if constexpr (sp::json::detail::is_unordered_map<T>()) {
            return "map&lt;" + fieldTypeName<typename T::key_type>() + ", " + fieldTypeName<typename T::mapped_type>() +
                   "&gt;";
        } else if constexpr (sp::json::detail::is_optional<T>()) {
            return "optional&lt;" + fieldTypeName<typename T::value_type>() + "&gt;";
        } else if constexpr (std::is_enum<T>()) {
            static const auto enumName = magic_enum::enum_type_name<T>();
            auto &typeName = references.emplace(enumName, typeid(T)).first->first;

            if constexpr (is_flags_enum<T>()) {
                return "enum flags [" + typeName + "](#" + typeName + "-type)";
            } else {
                return "enum [" + typeName + "](#" + typeName + "-type)";
            }
        } else {
            auto &metadata = ecs::StructMetadata::Get<T>();
            references.emplace(metadata.name, metadata.type);
            return "["s + metadata.name + "](#"s + metadata.name + "-type)";
        }
    }

public:
    void AddField(const ecs::StructField &field, const void *defaultPtr = nullptr) {
        ecs::GetFieldType(field.type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;

            static const T defaultStruct = {};
            auto &defaultValue = defaultPtr ? *reinterpret_cast<const T *>(defaultPtr) : defaultStruct;

            picojson::value defaultJson;
            if constexpr (!sp::json::detail::is_optional<T>()) {
                defaultJson = picojson::value(picojson::object());
                sp::json::Save(ecs::EntityScope(), defaultJson, defaultValue);
            }

            if (field.name.empty()) {
                if constexpr (std::is_enum<T>()) {
                    fields.emplace_back(DocField{"", fieldTypeName<T>(), field.desc, typeid(T), defaultJson});
                } else {
                    auto *metadata = ecs::StructMetadata::Get(typeid(T));
                    if (metadata) {
                        for (auto &field : metadata->fields) {
                            AddField(field, field.Access(&defaultValue));
                        }
                    } else {
                        fields.emplace_back(DocField{"", fieldTypeName<T>(), field.desc, typeid(T), defaultJson});
                    }
                }
            } else {
                fields.emplace_back(DocField{field.name, fieldTypeName<T>(), field.desc, typeid(T), defaultJson});
            }
        });
    }
};

enum class PageType {
    Component,
    Prefab,
    Script,
};

struct MarkdownContext {
    std::set<std::string> savedDocs;
    std::set<std::string> seeAlso;
    const PageType pageType;

    std::ofstream file;

    MarkdownContext(const std::filesystem::path &outputPath, PageType pageType) : pageType(pageType), file(outputPath) {
        Assertf(file, "Failed to open output file: '%s'", outputPath.string());
    }

    static std::string EscapeMarkdownString(const std::string &input) {
        size_t escapeCount = std::count(input.begin(), input.end(), '|');
        if (escapeCount == 0) return input;
        std::string result;
        result.reserve(input.length() + escapeCount * ("&#124;"s.length() - 1));
        for (auto &ch : input) {
            if (ch == '|') {
                result += "&#124;"s;
            }
            result += ch;
        }
        return result;
    }

    void SaveReferencedType(const std::string &refName, const std::type_index &refType) {
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
                     << EscapeMarkdownString(field.defaultValue.serialize()) << " | " << field.description << " |"
                     << std::endl;
            }
        }

        if (metadata && !metadata->description.empty()) {
            file << metadata->description << std::endl;
        }

        file << std::endl << "</div>" << std::endl;

        for (auto &[subRefName, subRefType] : refDocs.references) {
            if (!savedDocs.count(subRefName)) {
                SaveReferencedType(subRefName, subRefType);
            } else {
                seeAlso.emplace(subRefName);
            }
        }
    }

    template<typename ListType>
    void SavePage(const ListType &list, const CommonTypes *commonTypes = nullptr, CompList *otherList = nullptr) {
        if (commonTypes && !commonTypes->empty()) {
            file << std::endl << "<div class=\"component_definition\">" << std::endl << std::endl;
            file << "## Common Types" << std::endl;
            for (auto &type : *commonTypes) {
                auto *metadata = ecs::StructMetadata::Get(type);
                Assertf(metadata, "Type has no metadata: %s", type.name());
                SaveReferencedType(metadata->name, metadata->type);
            }
            file << std::endl << "</div>" << std::endl << std::endl;
        }

        for (auto &entry : list) {
            DocsStruct docs;

            const ecs::StructMetadata *metadata = nullptr;
            const std::string *name = nullptr;
            if constexpr (std::is_same_v<std::decay_t<decltype(entry)>, std::string>) {
                Assertf(pageType == PageType::Component,
                    "Unexpected page type: %s for %s",
                    magic_enum::enum_name(pageType),
                    typeid(ListType).name());
                if (otherList) sp::erase(*otherList, entry);

                auto &comp = *ecs::LookupComponent(entry);
                ecs::GetFieldType(comp.metadata.type, [&](auto *typePtr) {
                    using T = std::remove_pointer_t<decltype(typePtr)>;
                    static const T defaultComp = {};
                    for (auto &field : comp.metadata.fields) {
                        docs.AddField(field, field.Access(&defaultComp));
                    }
                });

                name = &entry;
                metadata = &comp.metadata;
            } else {
                Assertf(pageType == PageType::Prefab || pageType == PageType::Script,
                    "Unexpected page type: %s for %s",
                    magic_enum::enum_name(pageType),
                    typeid(ListType).name());

                const void *defaultScript = entry.second.context->GetDefault();
                for (auto &field : entry.second.context->metadata.fields) {
                    docs.AddField(field, field.Access(defaultScript));
                }

                name = &entry.first;
                metadata = &entry.second.context->metadata;
            }

            file << std::endl << "<div class=\"component_definition\">" << std::endl << std::endl;
            file << "## `" << *name << "` " << magic_enum::enum_name(pageType) << std::endl;

            if (docs.fields.empty()) {
                if (!metadata->description.empty()) {
                    file << metadata->description << std::endl;
                } else if (pageType == PageType::Component) {
                    file << "The `" << *name << "` component has no public fields" << std::endl;
                } else {
                    file << "The `" << *name << "` " << sp::to_lower_copy(magic_enum::enum_name(pageType))
                         << " has no configurable parameters" << std::endl;
                }
            } else if (docs.fields.size() == 1 && docs.fields.front().name.empty()) {
                if (pageType == PageType::Component) {
                    file << "The `" << *name << "` component has type: " << docs.fields.front().typeString << std::endl;
                }
                if (!metadata->description.empty()) {
                    file << metadata->description << std::endl;
                }
                if (pageType != PageType::Component) {
                    file << "The `" << *name << "` " << sp::to_lower_copy(magic_enum::enum_name(pageType))
                         << " has parameter type: " << docs.fields.front().typeString << std::endl;
                }
            } else {
                if (pageType != PageType::Component && !metadata->description.empty()) {
                    file << metadata->description << std::endl << std::endl;
                }

                auto fieldName = (pageType == PageType::Component ? "Field" : "Parameter");
                file << "| " << fieldName << " Name | Type | Default Value | Description |" << std::endl;
                file << "|------------|------|---------------|-------------|" << std::endl;
                for (auto &field : docs.fields) {
                    file << "| **" << field.name << "** | " << field.typeString << " | "
                         << EscapeMarkdownString(field.defaultValue.serialize()) << " | " << field.description << " |"
                         << std::endl;
                }

                if (pageType == PageType::Component && !metadata->description.empty()) {
                    file << metadata->description << std::endl;
                }
            }

            if (!docs.references.empty()) {
                seeAlso.clear();

                for (auto &[refName, refType] : docs.references) {
                    SaveReferencedType(refName, refType);
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
    }
};
