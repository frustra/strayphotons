/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "assets/JsonHelpers.hh"
#include "common/Common.hh"
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
        if constexpr (std::is_same<T, bool>()) {
            return "bool";
        } else if constexpr (std::is_same<T, int32_t>()) {
            return "int32";
        } else if constexpr (std::is_same<T, uint32_t>()) {
            return "uint32";
        } else if constexpr (std::is_same<T, size_t>()) {
            return "size_t";
        } else if constexpr (std::is_same<T, sp::angle_t>()) {
            return "float (degrees)";
        } else if constexpr (std::is_same<T, float>()) {
            return "float";
        } else if constexpr (std::is_same<T, double>()) {
            return "double";
        } else if constexpr (std::is_same<T, std::string>()) {
            return "string";
        } else if constexpr (sp::is_inline_string<T>()) {
            return "string (max " + std::to_string(T::max_size()) + " chars)";
        } else if constexpr (std::is_same<T, ecs::EventBytes>()) {
            return "bytes (max 256)";
        } else if constexpr (std::is_same<T, sp::color_t>()) {
            return "vec3 (red, green, blue)";
        } else if constexpr (std::is_same<T, sp::color_alpha_t>()) {
            return "vec4 (red, green, blue, alpha)";
        } else if constexpr (std::is_same<T, glm::quat>() || std::is_same<T, glm::mat3>()) {
            return "vec4 (angle_degrees, axis_x, axis_y, axis_z)";
        } else if constexpr (sp::is_glm_vec<T>()) {
            using U = typename T::value_type;

            if constexpr (std::is_same<U, float>()) {
                return "vec" + std::to_string(T::length());
            } else if constexpr (std::is_same<U, double>()) {
                return "dvec" + std::to_string(T::length());
            } else if constexpr (std::is_same<U, int>()) {
                return "ivec" + std::to_string(T::length());
            } else if constexpr (std::is_same<U, unsigned int>()) {
                return "uvec" + std::to_string(T::length());
            } else {
                return typeid(T).name();
            }
        } else if constexpr (sp::is_vector<T>()) {
            return "vector&lt;" + fieldTypeName<typename T::value_type>() + "&gt;";
        } else if constexpr (sp::is_pair<T>()) {
            return "pair&lt;" + fieldTypeName<typename T::first_type>() + ", " +
                   fieldTypeName<typename T::first_type>() + "&gt;";
        } else if constexpr (sp::is_unordered_flat_map<T>() || sp::is_unordered_node_map<T>()) {
            return "map&lt;" + fieldTypeName<typename T::key_type>() + ", " + fieldTypeName<typename T::mapped_type>() +
                   "&gt;";
        } else if constexpr (sp::is_optional<T>()) {
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

            if constexpr (std::is_default_constructible<T>()) {
                static const T defaultStruct = {};
                auto &defaultValue = defaultPtr ? *reinterpret_cast<const T *>(defaultPtr) : defaultStruct;

                picojson::value defaultJson;
                if constexpr (!sp::is_optional<T>() && !std::is_same<T, ecs::EventBytes>()) {
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
        result.reserve(input.length() + escapeCount);
        for (auto &ch : input) {
            if (ch == '|') {
                result += '\\';
            }
            result += ch;
        }
        return result;
    }

    void SaveReferencedType(std::ostream &out, const std::string &refName, const std::type_index &refType) {
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
                    if (metadata) {
                        auto it = metadata->enumMap.find((uint32_t)enumValue);
                        if (it != metadata->enumMap.end()) {
                            description = it->second.c_str();
                        }
                    }
                    refDocs.fields.emplace_back(DocField{std::string(enumName), "", description, typeid(T), {}});
                }
            } else if constexpr (std::is_default_constructible<T>()) {
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

        out << std::endl << "<div class=\"type_definition\">" << std::endl << std::endl;
        out << "### `" << refName << "` Type" << std::endl;
        if (isEnum) {
            if (metadata && !metadata->description.empty()) {
                out << std::endl << metadata->description << std::endl << std::endl;
            }
            if (isEnumFlags) {
                out << "This is an **enum flags** type. Multiple flags can be combined using the `|` "
                       "character (e.g. `\"One|Two\"` with no whitespace). Flag names are case-sensitive.  ";
                out << std::endl << "Enum flag names:" << std::endl;
            } else {
                out << "This is an **enum** type, and can be one of the following case-sensitive values:" << std::endl;
            }
            for (auto &field : refDocs.fields) {
                out << "- \"**" << field.name << "**\" - " << field.description << "" << std::endl;
            }
        } else {
            if (!refDocs.fields.empty()) {
                out << "| Field Name | Type | Default Value | Description |" << std::endl;
                out << "|------------|------|---------------|-------------|" << std::endl;
                for (auto &field : refDocs.fields) {
                    out << "| **" << field.name << "** | " << field.typeString << " | "
                        << EscapeMarkdownString(field.defaultValue.serialize()) << " | " << field.description << " |"
                        << std::endl;
                }
            }

            if (metadata && !metadata->description.empty()) {
                out << std::endl << metadata->description << std::endl;
            }
        }

        out << std::endl << "</div>" << std::endl;

        for (auto &[subRefName, subRefType] : refDocs.references) {
            if (!savedDocs.count(subRefName)) {
                SaveReferencedType(out, subRefName, subRefType);
            } else {
                seeAlso.emplace(subRefName);
            }
        }
    }

    template<typename ListType>
    void SavePage(const ListType &list, const CommonTypes *commonTypes = nullptr, CompList *otherList = nullptr) {
        std::stringstream commonTypesSS;
        if (commonTypes && !commonTypes->empty()) {
            for (auto &type : *commonTypes) {
                auto *metadata = ecs::StructMetadata::Get(type);
                Assertf(metadata, "Type has no metadata: %s", type.name());
                SaveReferencedType(commonTypesSS, metadata->name, metadata->type);
            }
        }

        for (auto &entry : list) {
            DocsStruct docs;

            const ecs::StructMetadata *metadata = nullptr;
            std::optional<std::string_view> name = {};
            if constexpr (std::is_same<std::decay_t<decltype(entry)>, std::string>()) {
                Assertf(pageType == PageType::Component,
                    "Unexpected page type: %s for %s",
                    magic_enum::enum_name(pageType),
                    typeid(ListType).name());
                if (otherList) sp::erase(*otherList, entry);

                auto &comp = *ecs::LookupComponent(entry);
                ecs::GetComponentType(comp.metadata.type, [&](auto *typePtr) {
                    using T = std::remove_pointer_t<decltype(typePtr)>;
                    static const T defaultComp = {};
                    for (auto &field : comp.metadata.fields) {
                        docs.AddField(field, field.Access(&defaultComp));
                    }
                });

                name = entry;
                metadata = &comp.metadata;
            } else {
                Assertf(pageType == PageType::Prefab || pageType == PageType::Script,
                    "Unexpected page type: %s for %s",
                    magic_enum::enum_name(pageType),
                    typeid(ListType).name());

                auto ctx = entry.second.context.lock();
                const void *defaultScript = ctx->GetDefault();
                for (auto &field : ctx->metadata.fields) {
                    docs.AddField(field, field.Access(defaultScript));
                }

                name = entry.first;
                metadata = &ctx->metadata;
            }

            file << std::endl << "<div class=\"component_definition\">" << std::endl << std::endl;
            file << "## `" << *name << "` " << magic_enum::enum_name(pageType) << std::endl << std::endl;

            if (docs.fields.empty()) {
                if (!metadata->description.empty()) {
                    file << metadata->description << std::endl << std::endl;
                } else if (pageType == PageType::Component) {
                    file << "The `" << *name << "` component has no public fields" << std::endl;
                } else {
                    file << "The `" << *name << "` " << sp::to_lower_copy(magic_enum::enum_name(pageType))
                         << " has no configurable parameters" << std::endl;
                }
            } else if (docs.fields.size() == 1 && docs.fields.front().name.empty()) {
                if (!metadata->description.empty()) {
                    file << metadata->description << std::endl << std::endl;
                }
                if (pageType == PageType::Component) {
                    file << "The `" << *name << "` component has type: " << docs.fields.front().typeString << std::endl;
                } else {
                    file << "The `" << *name << "` " << sp::to_lower_copy(magic_enum::enum_name(pageType))
                         << " has parameter type: " << docs.fields.front().typeString << std::endl;
                }
            } else {
                if (!metadata->description.empty()) {
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
            }

            if (!docs.references.empty()) {
                seeAlso.clear();

                for (auto &[refName, refType] : docs.references) {
                    SaveReferencedType(file, refName, refType);
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

        auto commonTypesStr = commonTypesSS.str();
        if (!commonTypesStr.empty()) {
            file << std::endl << "<div class=\"component_definition\">" << std::endl << std::endl;
            file << "## Additional Types" << std::endl;
            file << commonTypesStr;
            file << std::endl << "</div>" << std::endl << std::endl;
        }
    }
};
