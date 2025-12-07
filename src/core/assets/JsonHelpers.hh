/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "assets/AssetManager.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/EntityRef.hh"
#include "ecs/EventQueue.hh"
#include "ecs/SignalRef.hh"
#include "ecs/StructMetadata.hh"

#include <glm/glm.hpp>
#include <picojson.h>
#include <robin_hood.h>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace sp::json {
    namespace detail {
        template<glm::length_t L, typename T, glm::qualifier Q>
        inline bool LoadVec(glm::vec<L, T, Q> &dst, const picojson::value &src) {
            if (!src.is<picojson::array>()) return false;
            auto &values = src.get<picojson::array>();
            if (values.size() != L) {
                Errorf("Incorrect array size: %u, expected %u", values.size(), L);
                return false;
            }

            for (size_t i = 0; i < values.size(); i++) {
                if (!values[i].is<double>()) {
                    Errorf("Unexpected array value at index %u: %s", i, values[i].to_str());
                    dst = {};
                    return false;
                }
                dst[i] = (T)values[i].get<double>();
            }
            return true;
        }

        template<glm::length_t L, typename T, glm::qualifier Q>
        inline void SaveVec(picojson::value &dst, const glm::vec<L, T, Q> &src) {
            picojson::array vec(L);
            for (size_t i = 0; i < L; i++) {
                vec[i] = picojson::value((double)src[i]);
            }
            dst = picojson::value(vec);
        }
    } // namespace detail

    // Default Load handler for enums, and all integer and float types
    template<typename T>
    inline bool Load(T &dst, const picojson::value &src) {
        if constexpr (std::is_enum<T>()) {
            if (!src.is<std::string>()) return false;
            auto name = src.get<std::string>();
            if (name.empty()) {
                dst = {};
                return true;
            }
            std::optional<T> opt;
            if constexpr (is_flags_enum<T>()) {
                opt = magic_enum::enum_flags_cast<T>(name);
            } else {
                opt = magic_enum::enum_cast<T>(name);
            }
            if (!opt) {
                Errorf("Unknown enum value specified for %s: %s", typeid(T).name(), name);
                return false;
            }
            dst = *opt;
            return true;
        } else if constexpr (std::is_convertible_v<double, T> && std::is_convertible_v<T, double>) {
            if (!src.is<double>()) return false;
            dst = (T)src.get<double>();
            return true;
        } else {
            auto &metadata = ecs::StructMetadata::Get<T>();
            for (auto &field : metadata.fields) {
                if (!field.Load(&dst, src)) {
                    Errorf("Struct metadata %s has invalid field: %s = %s",
                        typeid(T).name(),
                        field.name,
                        src.serialize());
                    return false;
                }
            }
            ecs::StructMetadata::Load(dst, src);
            return true;
        }
    }

    // Load() specializations for native and complex types
    template<>
    inline bool Load(bool &dst, const picojson::value &src) {
        if (!src.is<bool>()) return false;
        dst = src.get<bool>();
        return true;
    }
    template<>
    inline bool Load(angle_t &dst, const picojson::value &src) {
        if (!src.is<double>()) return false;
        dst = glm::radians(src.get<double>());
        return true;
    }
    template<glm::length_t L, typename T, glm::qualifier Q>
    inline bool Load(glm::vec<L, T, Q> &dst, const picojson::value &src) {
        return detail::LoadVec<L, T, Q>(dst, src);
    }
    template<>
    inline bool Load(color_t &dst, const picojson::value &src) {
        return detail::LoadVec<3>(dst.color, src);
    }
    template<>
    inline bool Load(color_alpha_t &dst, const picojson::value &src) {
        return detail::LoadVec<4>(dst.color, src);
    }
    template<>
    inline bool Load(glm::quat &dst, const picojson::value &src) {
        glm::vec4 r(0);
        if (!detail::LoadVec<4>(r, src)) return false;
        dst = glm::angleAxis(glm::radians(r[0]), glm::normalize(glm::vec3(r[1], r[2], r[3])));
        return true;
    }
    template<>
    inline bool Load(glm::mat3 &dst, const picojson::value &src) {
        glm::quat q = glm::quat();
        if (!Load(q, src)) return false;
        dst = glm::mat3_cast(q);
        return true;
    }
    template<size_t MaxSize, typename CharT>
    inline bool Load(sp::InlineString<MaxSize, CharT> &dst, const picojson::value &src) {
        if (!src.is<std::string>()) return false;
        dst = src.get<std::string>();
        return true;
    }
    template<>
    inline bool Load(std::string &dst, const picojson::value &src) {
        if (!src.is<std::string>()) return false;
        dst = src.get<std::string>();
        return true;
    }
    template<>
    inline bool Load(ecs::Name &dst, const picojson::value &src) {
        if (!src.is<std::string>()) return false;
        auto name = src.get<std::string>();
        dst = ecs::Name(name, ecs::Name());
        return name.empty() == !dst;
    }
    template<>
    inline bool Load(ecs::Entity &dst, const picojson::value &src) {
        Errorf("json::Load unsupported type: ecs::Entity: %s", src.to_str());
        return false;
    }
    template<>
    inline bool Load(ecs::EntityRef &dst, const picojson::value &src) {
        if (!src.is<std::string>()) return false;
        auto &name = src.get<std::string>();
        dst = ecs::Name(name, ecs::Name());
        return name.empty() == !dst;
    }
    template<>
    inline bool Load(ecs::SignalRef &dst, const picojson::value &src) {
        if (!src.is<std::string>()) return false;
        auto &signalStr = src.get<std::string>();
        dst = ecs::SignalRef(signalStr);
        return signalStr.empty() == !dst;
    }
    template<typename T>
    inline bool Load(std::optional<T> &dst, const picojson::value &src) {
        T entry;
        if (!sp::json::Load(entry, src)) {
            dst.reset();
            return false;
        }
        dst = entry;
        return true;
    }
    template<typename A, typename B>
    inline bool Load(std::pair<A, B> &dst, const picojson::value &src) {
        if (!src.is<picojson::array>()) {
            Errorf("Unexpected type for pair<%s, %s>: %s", typeid(A).name(), typeid(B).name(), src.to_str());
            return false;
        }
        auto &arr = src.get<picojson::array>();
        if (arr.empty()) return true;
        if (arr.size() > 2) {
            Errorf("Too many values specified for pair<%s, %s>: %d", typeid(A).name(), typeid(B).name(), arr.size());
            return false;
        }
        if (!sp::json::Load<A>(dst.first, arr[0])) {
            return false;
        }
        if (arr.size() > 1) {
            if (!sp::json::Load<B>(dst.second, arr[1])) {
                return false;
            }
        } else {
            Errorf("Not enough values specified for pair<%s, %s>: %d", typeid(A).name(), typeid(B).name(), arr.size());
            return false;
        }
        return true;
    }
    template<typename T>
    inline bool Load(std::vector<T> &dst, const picojson::value &src) {
        dst.clear();
        if (src.is<picojson::array>()) {
            for (auto &p : src.get<picojson::array>()) {
                auto &entry = dst.emplace_back();
                if (!sp::json::Load(entry, p)) {
                    return false;
                }
            }
            return true;
        } else {
            if (src.is<picojson::object>()) {
                auto &obj = src.get<picojson::object>();
                // Empty object should represent an empty array, not a single default-initialized array entry
                if (obj.empty()) return true;
            }
            T entry;
            if (!sp::json::Load(entry, src)) {
                return false;
            }
            dst.emplace_back(entry);
            return true;
        }
    }
    template<typename K, typename T, typename H, typename E>
    inline bool Load(robin_hood::unordered_flat_map<K, T, H, E> &dst, const picojson::value &src) {
        if (!src.is<picojson::object>()) return false;
        dst.clear();
        for (auto &p : src.get<picojson::object>()) {
            if (!sp::json::Load(dst[p.first], p.second)) {
                return false;
            }
        }
        return true;
    }
    template<typename K, typename T, typename H, typename E>
    inline bool Load(robin_hood::unordered_node_map<K, T, H, E> &dst, const picojson::value &src) {
        if (!src.is<picojson::object>()) return false;
        dst.clear();
        for (auto &p : src.get<picojson::object>()) {
            if (!sp::json::Load(dst[p.first], p.second)) {
                return false;
            }
        }
        return true;
    }

    // Default Save handler for enums, and all integer and float types
    template<typename T>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const T &src) {
        if constexpr (std::is_enum<T>()) {
            if constexpr (is_flags_enum<T>()) {
                dst = picojson::value(std::string(magic_enum::enum_flags_name(src)));
            } else {
                dst = picojson::value(std::string(magic_enum::enum_name(src)));
            }
        } else if constexpr (std::is_convertible_v<double, T> && std::is_convertible_v<T, double>) {
            dst = picojson::value((double)src);
        } else if constexpr (std::is_default_constructible<T>()) {
            auto &metadata = ecs::StructMetadata::Get<T>();
            static const T defaultValue = {};
            for (auto &field : metadata.fields) {
                field.Save(s, dst, &src, &defaultValue);
            }
            ecs::StructMetadata::Save(s, dst, src, &defaultValue);
        }
    }

    // Save() specializations for native and complex types
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const bool &src) {
        dst = picojson::value(src);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const angle_t &src) {
        dst = picojson::value((double)src.degrees());
    }
    template<glm::length_t L, typename T, glm::qualifier Q>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const glm::vec<L, T, Q> &src) {
        detail::SaveVec<L, T, Q>(dst, src);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const color_t &src) {
        detail::SaveVec<3>(dst, src.color);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const color_alpha_t &src) {
        detail::SaveVec<4>(dst, src.color);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const glm::quat &src) {
        auto in = glm::normalize(src);
        glm::vec4 r(glm::degrees(glm::angle(in)), glm::normalize(glm::axis(in)));
        // Always serialize rotations between 0 and 180 degrees to keep them deterministic
        if (r[0] > 180.0f) r[0] -= 360.0f;
        if (r[0] < 0) r = -r;
        detail::SaveVec<4>(dst, r);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const glm::mat3 &src) {
        Save(s, dst, glm::quat_cast(src));
    }
    template<size_t MaxSize, typename CharT>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const sp::InlineString<MaxSize, CharT> &src) {
        dst = picojson::value(src.str());
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const std::string &src) {
        dst = picojson::value(src);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const ecs::Name &src) {
        auto strName = src.String();
        auto prefix = s.String();
        size_t prefixLen = 0;
        for (; prefixLen < strName.length() && prefixLen < prefix.length(); prefixLen++) {
            if (strName[prefixLen] != prefix[prefixLen]) break;
        }
        if (prefixLen != prefix.length() || prefixLen >= strName.length()) {
            dst = picojson::value(strName);
        } else {
            dst = picojson::value(strName.substr(prefixLen));
        }
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const ecs::Entity &src) {
        auto refName = ecs::EntityRef(src).Name();
        if (!refName && src) {
            Errorf("Can't serialize unnamed Entity: %s", std::to_string(src));
            return;
        }
        Save(s, dst, refName);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const ecs::EntityRef &src) {
        auto refName = src.Name();
        if (!refName && src) {
            Errorf("Can't serialize unnamed EntityRef: %s / %s",
                std::to_string(src.GetLive()),
                std::to_string(src.GetStaging()));
            return;
        }
        Save(s, dst, refName);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const ecs::SignalRef &src) {
        Save(s, dst, src.String());
    }
    template<typename T>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const std::optional<T> &src) {
        if (src) Save(s, dst, *src);
    }
    template<typename... Tn>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const std::variant<Tn...> &src) {
        std::visit(
            [&](auto &&value) {
                Save(s, dst, value);
            },
            src);
    }
    template<typename A, typename B>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const std::pair<A, B> &src) {
        picojson::array pair(2);
        Save(s, pair[0], src.first);
        Save(s, pair[1], src.second);
        dst = picojson::value(pair);
    }
    template<typename T>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const std::vector<T> &src) {
        if (src.size() == 1 && !sp::is_pair<T>()) {
            Save(s, dst, src[0]);
        } else {
            picojson::array vec(src.size());
            for (size_t i = 0; i < src.size(); i++) {
                Save(s, vec[i], src[i]);
            }
            dst = picojson::value(vec);
        }
    }
    template<typename T, typename H, typename E>
    inline void Save(const ecs::EntityScope &s,
        picojson::value &dst,
        const robin_hood::unordered_flat_map<std::string, T, H, E> &src) {
        picojson::object obj = {};
        for (auto &[key, value] : src) {
            Save(s, obj[key], value);
        }
        dst = picojson::value(obj);
    }
    template<typename T, typename H, typename E>
    inline void Save(const ecs::EntityScope &s,
        picojson::value &dst,
        const robin_hood::unordered_node_map<std::string, T, H, E> &src) {
        picojson::object obj = {};
        for (auto &[key, value] : src) {
            Save(s, obj[key], value);
        }
        dst = picojson::value(obj);
    }
    template<size_t MaxSize, typename T, typename H, typename E>
    inline void Save(const ecs::EntityScope &s,
        picojson::value &dst,
        const robin_hood::unordered_flat_map<InlineString<MaxSize>, T, H, E> &src) {
        picojson::object obj = {};
        for (auto &[key, value] : src) {
            Save(s, obj[key.str()], value);
        }
        dst = picojson::value(obj);
    }
    template<size_t MaxSize, typename T, typename H, typename E>
    inline void Save(const ecs::EntityScope &s,
        picojson::value &dst,
        const robin_hood::unordered_node_map<InlineString<MaxSize>, T, H, E> &src) {
        picojson::object obj = {};
        for (auto &[key, value] : src) {
            Save(s, obj[key.str()], value);
        }
        dst = picojson::value(obj);
    }

    template<typename T>
    inline bool Compare(const T &a, const T &b) {
        if constexpr (std::equality_comparable<T>) {
            return a == b;
        } else {
            auto &metadata = ecs::StructMetadata::Get<T>();
            for (auto &f : metadata.fields) {
                if (!f.Compare(&a, &b)) return false;
            }
            return true;
        }
    }

    template<typename T>
    inline bool SaveIfChanged(const ecs::EntityScope &s,
        picojson::value &dst,
        const std::string &field,
        const T &src,
        const T *def) {
        if (def && Compare(src, *def)) return false;

        picojson::value value;
        if constexpr (std::is_enum<T>()) {
            Save(s, value, src);
        } else if constexpr (std::is_same_v<T, ecs::EntityRef> || std::is_same_v<T, ecs::SignalRef> ||
                             std::is_same_v<T, ecs::SignalExpression>) {
            Save(s, value, src);
        } else if constexpr (std::is_convertible_v<double, T> && std::is_convertible_v<T, double>) {
            Save(s, value, src);
        } else {
            auto *metadata = ecs::StructMetadata::Get(typeid(T));
            if (metadata) {
                for (auto &f : metadata->fields) {
                    Assertf(f.type != typeid(T),
                        "Recursive field type found in: %s, field %s (%s)",
                        metadata->name,
                        f.name,
                        f.type.name());
                    f.Save(s, value, &src, def);
                }
                ecs::StructMetadata::Save(s, value, src, def);
            } else {
                Save(s, value, src);
            }
        }

        if (value.is<picojson::null>()) return false;
        if (!field.empty()) {
            if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
            dst.get<picojson::object>()[field] = value;
        } else if (value.is<picojson::object>()) {
            if (!dst.is<picojson::object>()) {
                dst = value;
            } else {
                dst.get<picojson::object>().merge(value.get<picojson::object>());
            }
        } else {
            dst = value;
        }
        return true;
    }

    template<typename T>
    inline bool SaveIfChanged(const ecs::EntityScope &s,
        picojson::value &dst,
        const std::string &field,
        const std::vector<T> &src,
        const std::vector<T> *def) {
        picojson::array arrayOut;
        for (auto &val : src) {
            // Skip if the value is the same as the default
            if (def && sp::contains(*def, val)) continue;

            picojson::value dstVal;
            Save(s, dstVal, val);
            if (dstVal.is<picojson::null>()) continue;
            arrayOut.emplace_back(dstVal);
        }
        if (def && arrayOut.empty()) return false;

        picojson::value valueOut;
        if (arrayOut.size() == 1 && !sp::is_pair<T>()) {
            valueOut = arrayOut.front();
        } else {
            valueOut = picojson::value(arrayOut);
        }
        if (!field.empty()) {
            if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
            dst.get<picojson::object>()[field] = valueOut;
        } else {
            dst = valueOut;
        }
        return true;
    }

    template<typename T>
    inline bool SaveIfChanged(const ecs::EntityScope &s,
        picojson::object &obj,
        const std::string &field,
        const T &src,
        const T *def) {
        if (def && Compare(src, *def)) return false;
        Assertf(!field.empty(), "json::SaveIfChanged provided object with no field");
        return SaveIfChanged(s, obj[field], "", src, def);
    }

    using SchemaTypeReferences = std::set<const ecs::StructMetadata *>;

    // Default JSON schema generation handler for most types
    template<typename T>
    inline void SaveSchema(picojson::value &dst, SchemaTypeReferences *references = nullptr, bool rootType = true) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &typeSchema = dst.get<picojson::object>();

        auto *metadata = ecs::StructMetadata::Get(typeid(T));
        if (metadata && !rootType) {
            if (references) {
                references->emplace(metadata);
            }
            typeSchema["$ref"] = picojson::value("#/definitions/"s + metadata->name);
        } else if constexpr (std::is_enum<T>()) {
            typeSchema["type"] = picojson::value("string");

            if constexpr (is_flags_enum<T>()) {
                // TODO: Generate a regex
            } else {
                static const auto names = magic_enum::enum_names<T>();
                picojson::array enumStrings(names.size());
                std::transform(names.begin(), names.end(), enumStrings.begin(), [](auto &name) {
                    return picojson::value(std::string(name));
                });
                typeSchema["enum"] = picojson::value(enumStrings);
            }
        } else if constexpr (std::is_integral_v<T>) {
            typeSchema["type"] = picojson::value("integer");

            if constexpr (std::is_unsigned_v<T>) {
                typeSchema["minimum"] = picojson::value(0.0);
            }
        } else if constexpr (std::is_floating_point_v<T>) {
            typeSchema["type"] = picojson::value("number");
        } else if constexpr (sp::is_inline_string<T>()) {
            typeSchema["type"] = picojson::value("string");
            typeSchema["maxLength"] = picojson::value((double)T::max_size());
        } else if constexpr (sp::is_glm_vec<T>()) {
            typeSchema["type"] = picojson::value("array");
            typeSchema["minItems"] = picojson::value((double)T::length());
            typeSchema["maxItems"] = picojson::value((double)T::length());
            SaveSchema<typename T::value_type>(typeSchema["items"], references, false);
        } else if constexpr (is_optional<T>()) {
            typeSchema["default"] = picojson::value();
            SaveSchema<typename T::value_type>(dst, references, false);
        } else if constexpr (sp::is_vector<T>()) {
            picojson::value subSchema;
            if constexpr (sp::is_pair<typename T::value_type>()) {
                picojson::value subSchemaA;
                SaveSchema<typename T::value_type::first_type>(subSchemaA, references, false);
                picojson::value subSchemaB;
                SaveSchema<typename T::value_type::second_type>(subSchemaB, references, false);

                picojson::object pairSchema;
                pairSchema["type"] = picojson::value("array");
                pairSchema["minItems"] = picojson::value(2.0);
                pairSchema["maxItems"] = picojson::value(2.0);
                pairSchema["items"] = picojson::value(false);
                picojson::array pairItems(2);
                pairItems[0] = subSchemaA;
                pairItems[1] = subSchemaB;
                pairSchema["prefixItems"] = picojson::value(pairItems);
                subSchema = picojson::value(pairSchema);
            } else {
                SaveSchema<typename T::value_type>(subSchema, references, false);
            }

            picojson::object arraySchema;
            arraySchema["type"] = picojson::value("array");
            arraySchema["items"] = subSchema;
            picojson::array anyOfArray(2);
            anyOfArray[0] = subSchema;
            anyOfArray[1] = picojson::value(arraySchema);
            typeSchema["anyOf"] = picojson::value(anyOfArray);
        } else if constexpr (sp::is_unordered_flat_map<T>() || sp::is_unordered_node_map<T>()) {
            typeSchema["type"] = picojson::value("object");
            static_assert(
                std::is_same<typename T::key_type, std::string>() || sp::is_inline_string<typename T::key_type>(),
                "Only string map keys are supported!");
            SaveSchema<typename T::mapped_type>(typeSchema["additionalProperties"], references, false);
        } else if constexpr (std::is_default_constructible<T>()) {
            if (!rootType) return;
            Assertf(metadata, "Unsupported type: %s", typeid(T).name());

            static const T defaultStruct = {};

            picojson::array allOfSchemas;
            picojson::object componentProperties;
            for (auto &field : metadata->fields) {
                picojson::value fieldSchema;
                field.DefineSchema(fieldSchema, references);

                if (field.name.empty()) {
                    allOfSchemas.emplace_back(std::move(fieldSchema));
                } else {
                    Assertf(fieldSchema.is<picojson::object>(),
                        "Expected subfield schema to be object: %s",
                        fieldSchema.to_str());
                    auto &fieldObj = fieldSchema.get<picojson::object>();
                    fieldObj["default"] = field.SaveDefault(ecs::EntityScope(), &defaultStruct);

                    fieldObj["description"] = picojson::value(field.desc);
                    componentProperties[field.name] = fieldSchema;
                }
            }
            if (!componentProperties.empty()) {
                typeSchema["type"] = picojson::value("object");
                typeSchema["properties"] = picojson::value(componentProperties);
            }

            if (!allOfSchemas.empty()) {
                if (typeSchema.empty() && allOfSchemas.size() == 1) {
                    typeSchema = allOfSchemas.front().get<picojson::object>();
                } else if (!typeSchema.empty()) {
                    allOfSchemas.emplace_back(std::move(typeSchema));
                    Assertf(typeSchema.empty(), "Move did not clear object");
                    typeSchema["allOf"] = picojson::value(allOfSchemas);
                }
            }
            picojson::value jsonDefault;
            Save(ecs::EntityScope(), jsonDefault, defaultStruct);
            if (!jsonDefault.is<picojson::null>()) typeSchema["default"] = jsonDefault;

            ecs::StructMetadata::DefineSchema<T>(dst, references);
        }
    }

    // SaveSchema() specializations for native and complex types
    template<>
    inline void SaveSchema<bool>(picojson::value &dst, SchemaTypeReferences *references, bool rootType) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &typeSchema = dst.get<picojson::object>();
        typeSchema["type"] = picojson::value("boolean");
    }

    template<>
    inline void SaveSchema<sp::angle_t>(picojson::value &dst, SchemaTypeReferences *references, bool rootType) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &typeSchema = dst.get<picojson::object>();
        typeSchema["type"] = picojson::value("number");
        typeSchema["description"] = picojson::value("An angle in degrees");
        typeSchema["exclusiveMinimum"] = picojson::value(-360.0);
        typeSchema["exclusiveMaximum"] = picojson::value(360.0);
    }

    template<>
    inline void SaveSchema<sp::color_t>(picojson::value &dst, SchemaTypeReferences *references, bool rootType) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &typeSchema = dst.get<picojson::object>();
        typeSchema["type"] = picojson::value("array");
        typeSchema["description"] = picojson::value(
            "An RGB color vector [red, green, blue] with values from 0.0 to 1.0");
        typeSchema["minItems"] = picojson::value(3.0);
        typeSchema["maxItems"] = picojson::value(3.0);
        picojson::object itemSchema;
        itemSchema["type"] = picojson::value("number");
        itemSchema["minimum"] = picojson::value(0.0);
        itemSchema["maximum"] = picojson::value(1.0);
        typeSchema["items"] = picojson::value(itemSchema);
    }

    template<>
    inline void SaveSchema<sp::color_alpha_t>(picojson::value &dst, SchemaTypeReferences *references, bool rootType) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &typeSchema = dst.get<picojson::object>();
        typeSchema["type"] = picojson::value("array");
        typeSchema["description"] = picojson::value(
            "An RGBA color vector [red, green, blue, alpha] with values from 0.0 to 1.0");
        typeSchema["minItems"] = picojson::value(4.0);
        typeSchema["maxItems"] = picojson::value(4.0);
        picojson::object itemSchema;
        itemSchema["type"] = picojson::value("number");
        itemSchema["minimum"] = picojson::value(0.0);
        itemSchema["maximum"] = picojson::value(1.0);
        typeSchema["items"] = picojson::value(itemSchema);
    }

    namespace detail {
        inline void saveRotationSchema(picojson::value &dst, SchemaTypeReferences *references) {
            if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
            auto &typeSchema = dst.get<picojson::object>();
            typeSchema["type"] = picojson::value("array");
            typeSchema["description"] = picojson::value(
                "A rotation around an axis, represented by the vector [angle_degrees, axis_x, axis_y, axis_z]. "
                "The axis does not to be normalized. As an example `[90, 1, 0, -1]` will rotate +90 degrees "
                "around an axis halfway between the +X and -Z directions. This is equivelent to `[-90, -1, 0, 1]`.");
            typeSchema["minItems"] = picojson::value(4.0);
            typeSchema["maxItems"] = picojson::value(4.0);
            picojson::array items(4);
            SaveSchema<sp::angle_t>(items[0], references, false);
            SaveSchema<float>(items[1], references, false);
            SaveSchema<float>(items[2], references, false);
            SaveSchema<float>(items[3], references, false);
            typeSchema["prefixItems"] = picojson::value(items);
        }
    } // namespace detail

    template<>
    inline void SaveSchema<glm::quat>(picojson::value &dst, SchemaTypeReferences *references, bool rootType) {
        detail::saveRotationSchema(dst, references);
    }
    template<>
    inline void SaveSchema<glm::mat3>(picojson::value &dst, SchemaTypeReferences *references, bool rootType) {
        detail::saveRotationSchema(dst, references);
    }

    template<>
    inline void SaveSchema<std::string>(picojson::value &dst, SchemaTypeReferences *references, bool rootType) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &typeSchema = dst.get<picojson::object>();
        typeSchema["type"] = picojson::value("string");
    }

    template<>
    inline void SaveSchema<ecs::Name>(picojson::value &dst, SchemaTypeReferences *references, bool rootType) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &typeSchema = dst.get<picojson::object>();
        typeSchema["type"] = picojson::value("string");
        typeSchema["description"] = picojson::value("An entity name in the form `<scene_name>:<entity_name>`");
        // TODO: Define a regex to validate the name
    }

    template<>
    inline void SaveSchema<ecs::Entity>(picojson::value &dst, SchemaTypeReferences *references, bool rootType) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &typeSchema = dst.get<picojson::object>();
        typeSchema["type"] = picojson::value("string");
        typeSchema["description"] = picojson::value("An entity name in the form `<scene_name>:<entity_name>`");
        // TODO: Define a regex to validate the reference
    }

    template<>
    inline void SaveSchema<ecs::EntityRef>(picojson::value &dst, SchemaTypeReferences *references, bool rootType) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &typeSchema = dst.get<picojson::object>();
        typeSchema["type"] = picojson::value("string");
        typeSchema["description"] = picojson::value("An entity name in the form `<scene_name>:<entity_name>`");
        // TODO: Define a regex to validate the reference
    }

    template<>
    inline void SaveSchema<ecs::SignalRef>(picojson::value &dst, SchemaTypeReferences *references, bool rootType) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &typeSchema = dst.get<picojson::object>();
        typeSchema["type"] = picojson::value("string");
        typeSchema["description"] = picojson::value(
            "An entity name + signal name in the form `<scene_name>:<entity_name>/<signal_name>`");
        // TODO: Define a regex to validate the reference
    }

    template<>
    inline void SaveSchema<ecs::EventData>(picojson::value &dst, SchemaTypeReferences *references, bool rootType) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
    }
} // namespace sp::json
