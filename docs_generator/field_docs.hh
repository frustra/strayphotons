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
