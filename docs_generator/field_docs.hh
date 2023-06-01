#pragma once

#include "assets/JsonHelpers.hh"
#include "ecs/Ecs.hh"
#include "ecs/StructFieldTypes.hh"

struct DocField {
    std::string name, typeString, description;
    std::type_index type;
    picojson::value defaultValue;
};

struct DocsContext {
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
            references.emplace(enumName, typeid(T));

            if constexpr (is_flags_enum<T>()) {
                return "enum flags `" + std::string(enumName) + "`";
            } else {
                return "enum `" + std::string(enumName) + "`";
            }
        } else {
            auto &metadata = ecs::StructMetadata::Get<T>();
            references.emplace(metadata.name, metadata.type);
            return "`"s + metadata.name + "`";
        }
    }

public:
    template<typename T>
    void SaveFields() {
        static const T defaultStruct = {};

        auto *metadataPtr = ecs::StructMetadata::Get(typeid(T));
        if (!metadataPtr) {
            Errorf("Unknown saveFields metadata: %s", typeid(T).name());
            return;
        }
        auto &metadata = *metadataPtr;

        if constexpr (std::is_same_v<T, ecs::Name>) {
            fields.emplace_back(
                DocField{"", fieldTypeName<std::string>(), "No description", typeid(ecs::Name), picojson::value("")});
        } else {
            for (auto &field : metadata.fields) {
                const void *fieldPtr = field.Access(&defaultStruct);

                ecs::GetFieldType(field.type, fieldPtr, [&](auto &defaultValue) {
                    using U = std::decay_t<decltype(defaultValue)>;

                    picojson::value output;
                    sp::json::SaveIfChanged<U>(ecs::EntityScope(), output, "", defaultValue, nullptr);

                    if (field.name.empty()) {
                        if constexpr (std::is_enum<U>()) {
                            fields.emplace_back(DocField{"",
                                fieldTypeName<U>(),
                                "The `"s + metadata.name + "` component is serialized directly as an enum string",
                                typeid(U),
                                output});
                        } else if constexpr (sp::is_vector<U>()) {
                            fields.emplace_back(DocField{"",
                                fieldTypeName<U>(),
                                "The `"s + metadata.name + "` component is serialized directly as an array",
                                typeid(U),
                                output});
                        } else if constexpr (sp::json::detail::is_unordered_map<U>()) {
                            fields.emplace_back(DocField{"",
                                fieldTypeName<U>(),
                                "The `"s + metadata.name + "` component is serialized directly as a map",
                                typeid(U),
                                output});
                        } else if constexpr (!std::is_same_v<T, U>) {
                            SaveFields<U>();
                        } else {
                            Abortf("Unknown self reference state: %s", metadata.name);
                        }
                    } else {
                        fields.emplace_back(DocField{field.name, fieldTypeName<U>(), field.desc, typeid(U), output});
                    }
                });
            }
        }
    }
};
