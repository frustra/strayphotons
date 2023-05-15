#pragma once

#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EntityRef.hh"
#include "ecs/StructMetadata.hh"

#include <glm/glm.hpp>
#include <picojson/picojson.h>
#include <robin_hood.h>
#include <string>
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
    inline bool Load(const ecs::EntityScope &s, T &dst, const picojson::value &src) {
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
        } else if constexpr (std::is_convertible_v<double, T>) {
            if (!src.is<double>()) return false;
            dst = (T)src.get<double>();
            return true;
        } else {
            auto &metadata = ecs::StructMetadata::Get<T>();
            for (auto &field : metadata.fields) {
                if (!field.Load(s, &dst, src)) {
                    Errorf("Struct metadata %s has invalid field: %s", typeid(T).name(), field.name);
                    return false;
                }
            }
            ecs::StructMetadata::Load(s, dst, src);
            return true;
        }
    }

    // Load() specializations for native and complex types
    template<>
    inline bool Load(const ecs::EntityScope &s, bool &dst, const picojson::value &src) {
        if (!src.is<bool>()) return false;
        dst = src.get<bool>();
        return true;
    }
    template<>
    inline bool Load(const ecs::EntityScope &s, angle_t &dst, const picojson::value &src) {
        if (!src.is<double>()) return false;
        dst = glm::radians(src.get<double>());
        return true;
    }
    template<glm::length_t L, typename T, glm::qualifier Q>
    inline bool Load(const ecs::EntityScope &s, glm::vec<L, T, Q> &dst, const picojson::value &src) {
        return detail::LoadVec<L, T, Q>(dst, src);
    }
    template<>
    inline bool Load(const ecs::EntityScope &s, color_t &dst, const picojson::value &src) {
        return detail::LoadVec<3>(dst.color, src);
    }
    template<>
    inline bool Load(const ecs::EntityScope &s, color_alpha_t &dst, const picojson::value &src) {
        return detail::LoadVec<4>(dst.color, src);
    }
    template<>
    inline bool Load(const ecs::EntityScope &s, glm::quat &dst, const picojson::value &src) {
        glm::vec4 r;
        if (!detail::LoadVec<4>(r, src)) return false;
        dst = glm::angleAxis(glm::radians(r[0]), glm::normalize(glm::vec3(r[1], r[2], r[3])));
        return true;
    }
    template<>
    inline bool Load(const ecs::EntityScope &s, std::string &dst, const picojson::value &src) {
        if (!src.is<std::string>()) return false;
        dst = src.get<std::string>();
        return true;
    }
    template<>
    inline bool Load(const ecs::EntityScope &s, ecs::Name &dst, const picojson::value &src) {
        if (!src.is<std::string>()) return false;
        auto name = src.get<std::string>();
        dst = ecs::Name(name, s);
        return name.empty() == !dst;
    }
    template<>
    inline bool Load(const ecs::EntityScope &s, ecs::EntityRef &dst, const picojson::value &src) {
        if (!src.is<std::string>()) return false;
        auto name = src.get<std::string>();
        dst = ecs::Name(name, s);
        return name.empty() == !dst;
    }
    template<typename T>
    inline bool Load(const ecs::EntityScope &s, std::optional<T> &dst, const picojson::value &src) {
        T entry;
        if (!sp::json::Load(s, entry, src)) {
            dst.reset();
            return false;
        }
        dst = entry;
        return true;
    }
    template<typename T>
    inline bool Load(const ecs::EntityScope &s, std::vector<T> &dst, const picojson::value &src) {
        dst.clear();
        if (src.is<picojson::array>()) {
            for (auto &p : src.get<picojson::array>()) {
                auto &entry = dst.emplace_back();
                if (!sp::json::Load(s, entry, p)) {
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
            if (!sp::json::Load(s, entry, src)) {
                return false;
            }
            dst.emplace_back(entry);
            return true;
        }
    }
    template<typename T>
    inline bool Load(const ecs::EntityScope &s,
        robin_hood::unordered_flat_map<std::string, T> &dst,
        const picojson::value &src) {
        if (!src.is<picojson::object>()) return false;
        dst.clear();
        for (auto &p : src.get<picojson::object>()) {
            if (!sp::json::Load(s, dst[p.first], p.second)) {
                return false;
            }
        }
        return true;
    }
    template<typename T>
    inline bool Load(const ecs::EntityScope &s,
        robin_hood::unordered_node_map<std::string, T> &dst,
        const picojson::value &src) {
        if (!src.is<picojson::object>()) return false;
        dst.clear();
        for (auto &p : src.get<picojson::object>()) {
            if (!sp::json::Load(s, dst[p.first], p.second)) {
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
        } else {
            auto &metadata = ecs::StructMetadata::Get<T>();
            static const T defaultValue = {};
            for (auto &field : metadata.fields) {
                field.Save(s, dst, &src, &defaultValue);
            }
            ecs::StructMetadata::Save(s, dst, src, defaultValue);
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
    template<typename T>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const std::vector<T> &src) {
        if (src.size() == 1) {
            Save(s, dst, src[0]);
        } else {
            picojson::array vec(src.size());
            for (size_t i = 0; i < src.size(); i++) {
                Save(s, vec[i], src[i]);
            }
            dst = picojson::value(vec);
        }
    }
    template<typename T>
    inline void Save(const ecs::EntityScope &s,
        picojson::value &dst,
        const robin_hood::unordered_flat_map<std::string, T> &src) {
        picojson::object obj = {};
        for (auto &[key, value] : src) {
            Save(s, obj[key], value);
        }
        dst = picojson::value(obj);
    }
    template<typename T>
    inline void Save(const ecs::EntityScope &s,
        picojson::value &dst,
        const robin_hood::unordered_node_map<std::string, T> &src) {
        picojson::object obj = {};
        for (auto &[key, value] : src) {
            Save(s, obj[key], value);
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
        const T &def) {
        if (Compare(src, def)) return false;

        picojson::value value;
        auto *metadata = ecs::StructMetadata::Get(typeid(T));
        if (metadata) {
            for (auto &f : metadata->fields) {
                f.Save(s, value, &src, &def);
            }
            ecs::StructMetadata::Save(s, value, src, def);
        } else {
            Save(s, value, src);
        }

        if (value.is<picojson::null>()) return false;
        if (!field.empty()) {
            if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
            dst.get<picojson::object>()[field] = value;
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
        const std::vector<T> &def) {
        picojson::array arrayOut;
        for (auto &val : src) {
            // Skip if the value is the same as the default
            if (sp::contains(def, val)) continue;

            picojson::value dstVal;
            Save(s, dstVal, val);
            if (dstVal.is<picojson::null>()) continue;
            arrayOut.emplace_back(dstVal);
        }
        if (arrayOut.empty()) return false;

        picojson::value valueOut;
        if (arrayOut.size() == 1) {
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
        const T &def) {
        if (Compare(src, def)) return false;
        Assertf(!field.empty(), "json::SaveIfChanged provided object with no field");
        return SaveIfChanged(s, obj[field], "", src, def);
    }
} // namespace sp::json
