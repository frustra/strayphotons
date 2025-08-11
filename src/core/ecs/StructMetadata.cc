/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "StructMetadata.hh"

#include "assets/JsonHelpers.hh"
#include "common/Common.hh"
#include "ecs/StructFieldTypes.hh"

#include <cstring>

namespace ecs {
    uint32_t GetFieldTypeIndex(const std::type_index &idx) {
        if (idx == typeid(void)) return 0;
        return GetFieldType(idx, [](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;
            return GetFieldTypeIndex<T>();
        });
    }

    std::type_index GetFieldTypeIndex(uint32_t typeIndex) {
        if (typeIndex == 0) return typeid(void);
        return GetFieldType(typeIndex, [](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;
            return std::type_index(typeid(T));
        });
    }

    typedef std::map<std::type_index, const StructMetadata *> MetadataTypeMap;
    MetadataTypeMap *metadataTypeMap = nullptr;

    const StructMetadata *StructMetadata::Get(const std::type_index &idx) {
        if (metadataTypeMap == nullptr) metadataTypeMap = new MetadataTypeMap();

        auto it = metadataTypeMap->find(idx);
        if (it != metadataTypeMap->end()) return it->second;
        return nullptr;
    }

    void StructMetadata::Register(const std::type_index &idx, const StructMetadata *comp) {
        if (metadataTypeMap == nullptr) metadataTypeMap = new MetadataTypeMap();
        metadataTypeMap->insert_or_assign(idx, comp);
    }

    template<typename, typename>
    struct has_type;
    template<typename T, typename... Un>
    struct has_type<T, std::tuple<Un...>> : std::disjunction<std::is_same<T, Un>...> {};

    const auto &getUndefinedFieldValues() {
        static const auto undefinedValues = std::make_tuple(sp::angle_t(-INFINITY),
            -std::numeric_limits<float>::infinity(),
            -std::numeric_limits<double>::infinity(),
            glm::vec2(-INFINITY),
            glm::vec3(-INFINITY),
            glm::vec4(-INFINITY),
            glm::ivec2(std::numeric_limits<int32_t>::min()),
            glm::ivec3(std::numeric_limits<int32_t>::min()),
            glm::ivec4(std::numeric_limits<int32_t>::min()),
            sp::color_t(glm::vec3(-INFINITY)),
            sp::color_alpha_t(glm::vec4(-INFINITY)),
            glm::quat(-INFINITY, -INFINITY, -INFINITY, -INFINITY));
        return undefinedValues;
    };

    using UndefinedValuesTuple = std::remove_cvref_t<std::invoke_result_t<decltype(&getUndefinedFieldValues)>>;

    template<typename T>
    inline static constexpr bool isFieldUndefined(const T &value) {
        if constexpr (has_type<T, UndefinedValuesTuple>()) {
            return value == std::get<T>(getUndefinedFieldValues());
        } else {
            return false;
        }
    }

    void StructField::InitUndefined(void *dstStruct, const void *defaultStruct) const {
        auto *field = static_cast<char *>(dstStruct) + offset;
        auto *defaultField = static_cast<const char *>(defaultStruct) + offset;

        GetFieldType(type, field, [&](auto &value) {
            using T = std::decay_t<decltype(value)>;

            auto &defaultValue = *reinterpret_cast<const T *>(defaultField);

            if constexpr (has_type<T, UndefinedValuesTuple>()) {
                value = std::get<T>(getUndefinedFieldValues());
            } else if constexpr (std::is_default_constructible<T>()) {
                value = defaultValue;
            } else {
                Abortf("StructField::InitUndefined called on uninitializable type: %s", typeid(T).name());
            }
        });
    }

    void StructField::DefineSchema(picojson::value &dst, sp::json::SchemaTypeReferences *references) const {
        GetFieldType(type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;
            if constexpr (std::is_default_constructible<T>()) {
                sp::json::SaveSchema<T>(dst, references, false);
            } else {
                Abortf("StructField::DefineSchema called on uninitializable type: %s", typeid(T).name());
            }
        });
    }

    picojson::value StructField::SaveDefault(const EntityScope &scope, const void *defaultStruct) const {
        picojson::value result;
        auto *field = static_cast<const char *>(defaultStruct) + offset;
        GetFieldType(type, field, [&](auto &value) {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_default_constructible<T>()) {
                sp::json::SaveIfChanged<T>(scope, result, "", value, nullptr);
            } else {
                Abortf("StructField::SaveDefault called on uninitializable type: %s", typeid(T).name());
            }
        });
        return result;
    }

    void StructField::SetScope(void *dstStruct, const EntityScope &scope) const {
        auto *field = static_cast<char *>(dstStruct) + offset;

        GetFieldType(type, field, [&](auto &dstValue) {
            scope::SetScope(dstValue, scope);
        });
    }

    bool StructField::Compare(const void *a, const void *b) const {
        auto *fieldA = static_cast<const char *>(a) + offset;
        auto *fieldB = static_cast<const char *>(b) + offset;

        return GetFieldType(type, fieldA, [&](auto &valueA) -> bool {
            using T = std::decay_t<decltype(valueA)>;
            auto &valueB = *reinterpret_cast<const T *>(fieldB);

            if constexpr (std::equality_comparable<T>) {
                return valueA == valueB;
            } else {
                Abortf("StructField::Compare called on unsupported type: %s", type.name());
            }
        });
    }

    bool StructField::Load(void *dstStruct, const picojson::value &src) const {
        if (!(actions & FieldAction::AutoLoad)) return true;

        auto *dstfield = static_cast<char *>(dstStruct) + offset;
        auto *srcField = &src;

        if (!name.empty()) {
            if (!src.is<picojson::object>()) {
                // Silently leave missing fields as default
                return true;
            }
            auto &obj = src.get<picojson::object>();
            auto it = obj.find(name);
            if (it == obj.end()) {
                // Silently leave missing fields as default
                return true;
            }
            srcField = &it->second;
        }

        return GetFieldType(type, dstfield, [&](auto &dstValue) {
            if (!sp::json::Load(dstValue, *srcField)) {
                Errorf("Invalid %s field value: %s", type.name(), srcField->serialize());
                return false;
            }
            return true;
        });
    }

    void StructField::Save(const EntityScope &scope,
        picojson::value &dst,
        const void *srcStruct,
        const void *defaultStruct) const {
        if (!(actions & FieldAction::AutoSave)) return;

        auto *field = static_cast<const char *>(srcStruct) + offset;
        auto *defaultField = defaultStruct ? static_cast<const char *>(defaultStruct) + offset : nullptr;

        GetFieldType(type, field, [&](auto &value) {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_default_constructible<T>()) {
                auto *defaultValue = reinterpret_cast<const T *>(defaultField);
                sp::json::SaveIfChanged(scope, dst, name, value, defaultValue);
            } else {
                Abortf("StructField::Save called on uninitializable type: %s", typeid(T).name());
            }
        });
    }

    void StructField::Apply(void *dstStruct, const void *srcStruct, const void *defaultStruct) const {
        if (!(actions & FieldAction::AutoApply)) return;

        auto *dstField = static_cast<char *>(dstStruct) + offset;
        auto *srcField = static_cast<const char *>(srcStruct) + offset;
        auto *defaultField = static_cast<const char *>(defaultStruct) + offset;

        GetFieldType(type, dstField, [&](auto &dstValue) {
            using T = std::decay_t<decltype(dstValue)>;

            auto &srcValue = *reinterpret_cast<const T *>(srcField);
            auto &defaultValue = *reinterpret_cast<const T *>(defaultField);

            if constexpr (std::equality_comparable<T>) {
                if (dstValue == defaultValue && !isFieldUndefined(srcValue)) dstValue = srcValue;
            } else {
                Abortf("StructField::Apply called on unsupported type: %s", type.name());
            }
        });
    }
} // namespace ecs
