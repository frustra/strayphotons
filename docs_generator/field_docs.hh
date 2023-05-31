#pragma once

#include "assets/JsonHelpers.hh"
#include "ecs/Ecs.hh"
#include "ecs/StructFieldTypes.hh"

template<typename T>
struct is_optional : std::false_type {};
template<typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template<typename T>
struct is_unordered_map : std::false_type {};
template<typename K, typename V>
struct is_unordered_map<robin_hood::unordered_flat_map<K, V>> : std::true_type {};
template<typename K, typename V>
struct is_unordered_map<robin_hood::unordered_node_map<K, V>> : std::true_type {};

struct DocField {
    std::string name, typeString, description;
    std::type_index type;
    picojson::value defaultValue;
};

struct DocsContext {
    std::vector<DocField> fields;
    std::map<std::string, std::type_index> references;
    std::map<std::string, picojson::object> jsonDefinitions;

    template<typename T>
    std::string FieldTypeName(picojson::object **schemaOut = nullptr) {
        std::string typeName;
        picojson::object typeSchema;
        if constexpr (std::is_same_v<T, bool>) {
            typeName = "bool";
            typeSchema["type"] = picojson::value("boolean");
        } else if constexpr (std::is_same_v<T, int32_t>) {
            typeName = "int32";
            typeSchema["type"] = picojson::value("integer");
            // typeSchema["minimum"] = picojson::value((double)std::numeric_limits<T>::lowest());
            // typeSchema["maximum"] = picojson::value((double)std::numeric_limits<T>::max());
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            typeName = "uint32";
            typeSchema["type"] = picojson::value("integer");
            // typeSchema["minimum"] = picojson::value((double)std::numeric_limits<T>::lowest());
            // typeSchema["maximum"] = picojson::value((double)std::numeric_limits<T>::max());
        } else if constexpr (std::is_same_v<T, size_t>) {
            typeName = "size_t";
            typeSchema["type"] = picojson::value("integer");
            // typeSchema["minimum"] = picojson::value((double)std::numeric_limits<T>::lowest());
            // typeSchema["maximum"] = picojson::value((double)std::numeric_limits<T>::max());
        } else if constexpr (std::is_same_v<T, sp::angle_t>) {
            typeName = "float (degrees)";
            typeSchema["type"] = picojson::value("number");
            typeSchema["exclusiveMinimum"] = picojson::value(-360.0);
            typeSchema["exclusiveMaximum"] = picojson::value(360.0);
        } else if constexpr (std::is_same_v<T, float>) {
            typeName = "float";
            typeSchema["type"] = picojson::value("number");
            // typeSchema["minimum"] = picojson::value((double)std::numeric_limits<T>::lowest());
            // typeSchema["maximum"] = picojson::value((double)std::numeric_limits<T>::max());
        } else if constexpr (std::is_same_v<T, double>) {
            typeName = "double";
            typeSchema["type"] = picojson::value("number");
        } else if constexpr (std::is_same_v<T, std::string>) {
            typeName = "string";
            typeSchema["type"] = picojson::value("string");
        } else if constexpr (std::is_same_v<T, sp::color_t>) {
            typeName = "vec3 (red, green, blue)";

            typeSchema["type"] = picojson::value("array");
            typeSchema["minItems"] = picojson::value(3.0);
            typeSchema["maxItems"] = picojson::value(3.0);
            picojson::object itemSchema;
            itemSchema["type"] = picojson::value("number");
            itemSchema["minimum"] = picojson::value(0.0);
            itemSchema["maximum"] = picojson::value(1.0);
            typeSchema["items"] = picojson::value(itemSchema);
        } else if constexpr (std::is_same_v<T, sp::color_alpha_t>) {
            typeName = "vec4 (red, green, blue, alpha)";

            typeSchema["type"] = picojson::value("array");
            typeSchema["minItems"] = picojson::value(4.0);
            typeSchema["maxItems"] = picojson::value(4.0);
            picojson::object itemSchema;
            itemSchema["type"] = picojson::value("number");
            itemSchema["minimum"] = picojson::value(0.0);
            itemSchema["maximum"] = picojson::value(1.0);
            typeSchema["items"] = picojson::value(itemSchema);
        } else if constexpr (std::is_same_v<T, glm::quat> || std::is_same_v<T, glm::mat3>) {
            typeName = "vec4 (angle_degrees, axis_x, axis_y, axis_z)";

            typeSchema["type"] = picojson::value("array");
            typeSchema["minItems"] = picojson::value(4.0);
            typeSchema["maxItems"] = picojson::value(4.0);
            picojson::object angleSchema;
            angleSchema["type"] = picojson::value("number");
            angleSchema["exclusiveMinimum"] = picojson::value(-360.0);
            angleSchema["exclusiveMaximum"] = picojson::value(360.0);
            picojson::object axisSchema;
            axisSchema["type"] = picojson::value("number");
            axisSchema["minimum"] = picojson::value(-1.0);
            axisSchema["maximum"] = picojson::value(1.0);
            picojson::array items(4);
            items[0] = picojson::value(angleSchema);
            items[1] = picojson::value(axisSchema);
            items[2] = picojson::value(axisSchema);
            items[3] = picojson::value(axisSchema);
            typeSchema["prefixItems"] = picojson::value(items);
        } else if constexpr (sp::is_glm_vec<T>()) {
            using U = typename T::value_type;

            if constexpr (std::is_same_v<U, float>) {
                typeName = "vec" + std::to_string(T::length());
            } else if constexpr (std::is_same_v<U, double>) {
                typeName = "dvec" + std::to_string(T::length());
            } else if constexpr (std::is_same_v<U, int>) {
                typeName = "ivec" + std::to_string(T::length());
            } else if constexpr (std::is_same_v<U, unsigned int>) {
                typeName = "uvec" + std::to_string(T::length());
            } else {
                typeName = typeid(T).name();
            }

            typeSchema["type"] = picojson::value("array");
            typeSchema["minItems"] = picojson::value((double)T::length());
            typeSchema["maxItems"] = picojson::value((double)T::length());
            picojson::object itemSchema;
            if constexpr (std::is_integral_v<U>) {
                itemSchema["type"] = picojson::value("integer");
            } else {
                itemSchema["type"] = picojson::value("number");
            }
            // itemSchema["minimum"] = picojson::value((double)std::numeric_limits<U>::lowest());
            // itemSchema["maximum"] = picojson::value((double)std::numeric_limits<U>::max());
            typeSchema["items"] = picojson::value(itemSchema);
        } else if constexpr (sp::is_vector<T>()) {
            picojson::object *subSchema = nullptr;
            typeName = "vector&lt;" + FieldTypeName<typename T::value_type>(&subSchema) + "&gt;";
            Assertf(subSchema, "FieldTypeName return null subSchema: %s", typeName);

            picojson::object arraySchema;
            arraySchema["type"] = picojson::value("array");
            arraySchema["items"] = picojson::value(*subSchema);
            picojson::array anyOfArray(2);
            anyOfArray[0] = picojson::value(*subSchema);
            anyOfArray[1] = picojson::value(arraySchema);
            typeSchema["anyOf"] = picojson::value(anyOfArray);
        } else if constexpr (is_unordered_map<T>()) {
            picojson::object *subSchema = nullptr;
            typeName = "map&lt;" + FieldTypeName<typename T::key_type>() + ", " +
                       FieldTypeName<typename T::mapped_type>(&subSchema) + "&gt;";
            Assertf(subSchema, "FieldTypeName return null subSchema: %s", typeName);

            static_assert(std::is_same_v<typename T::key_type, std::string>, "Only string map keys are supported!");
            typeSchema["type"] = picojson::value("object");
            typeSchema["additionalProperties"] = picojson::value(*subSchema);
        } else if constexpr (is_optional<T>()) {
            picojson::object *subSchema = nullptr;
            typeName = "optional&lt;" + FieldTypeName<typename T::value_type>(&subSchema) + "&gt;";
            Assertf(subSchema, "FieldTypeName return null subSchema: %s", typeName);
            typeSchema = *subSchema;
        } else if constexpr (std::is_enum<T>()) {
            static const auto enumName = magic_enum::enum_type_name<T>();
            references.emplace(enumName, typeid(T));

            if constexpr (is_flags_enum<T>()) {
                typeName = "enum flags `" + std::string(enumName) + "`";
                typeSchema["$ref"] = picojson::value("#/definitions/" + std::string(enumName));
            } else {
                typeName = "enum `" + std::string(enumName) + "`";
                typeSchema["$ref"] = picojson::value("#/definitions/" + std::string(enumName));
            }
        } else {
            auto &metadata = ecs::StructMetadata::Get<T>();
            references.emplace(metadata.name, metadata.type);
            typeName = "`"s + metadata.name + "`";

            typeSchema["$ref"] = picojson::value("#/definitions/"s + metadata.name);
        }
        auto [it, _] = jsonDefinitions.emplace(typeName, typeSchema);
        if (schemaOut) {
            *schemaOut = &it->second;
        }
        return typeName;
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
                DocField{"", FieldTypeName<ecs::Name>(), "No description", typeid(ecs::Name), picojson::value("")});
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
                                FieldTypeName<U>(),
                                "The `"s + metadata.name + "` component is serialized directly as an enum string",
                                typeid(U),
                                output});
                        } else if constexpr (sp::is_vector<U>()) {
                            fields.emplace_back(DocField{"",
                                FieldTypeName<U>(),
                                "The `"s + metadata.name + "` component is serialized directly as an array",
                                typeid(U),
                                output});
                        } else if constexpr (is_unordered_map<U>()) {
                            fields.emplace_back(DocField{"",
                                FieldTypeName<U>(),
                                "The `"s + metadata.name + "` component is serialized directly as a map",
                                typeid(U),
                                output});
                        } else if constexpr (!std::is_same_v<T, U>) {
                            SaveFields<U>();
                        } else {
                            Abortf("Unknown self reference state: %s", metadata.name);
                        }
                    } else {
                        fields.emplace_back(DocField{field.name, FieldTypeName<U>(), field.desc, typeid(U), output});
                    }
                });
            }
        }
    }
};
