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
#include <vector>

namespace sp::json {
    namespace detail {
        template<size_t N, typename T, glm::precision P>
        inline bool LoadVec(glm::vec<N, T, P> &dst, const picojson::value &src) {
            if (!src.is<picojson::array>()) return false;
            auto &values = src.get<picojson::array>();
            if (values.size() != N) {
                Errorf("Incorrect array size: %u, expected %u", values.size(), N);
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

        template<size_t N, typename T, glm::precision P>
        inline void SaveVec(picojson::value &dst, const glm::vec<N, T, P> &src) {
            picojson::array vec(N);
            for (size_t i = 0; i < N; i++) {
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
            auto opt = magic_enum::enum_cast<T>(name);
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
    template<>
    inline bool Load(const ecs::EntityScope &s, glm::vec2 &dst, const picojson::value &src) {
        return detail::LoadVec<2>(dst, src);
    }
    template<>
    inline bool Load(const ecs::EntityScope &s, glm::vec3 &dst, const picojson::value &src) {
        return detail::LoadVec<3>(dst, src);
    }
    template<>
    inline bool Load(const ecs::EntityScope &s, glm::vec4 &dst, const picojson::value &src) {
        return detail::LoadVec<4>(dst, src);
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
    inline bool Load(const ecs::EntityScope &s, glm::ivec2 &dst, const picojson::value &src) {
        return detail::LoadVec<2>(dst, src);
    }
    template<>
    inline bool Load(const ecs::EntityScope &s, glm::ivec3 &dst, const picojson::value &src) {
        return detail::LoadVec<3>(dst, src);
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
    inline bool Load(const ecs::EntityScope &s, ecs::EntityRef &dst, const picojson::value &src) {
        if (!src.is<std::string>()) return false;
        auto name = src.get<std::string>();
        dst = ecs::Name(name, s.prefix);
        return name.empty() == !dst;
    }
    template<typename T>
    inline bool Load(const ecs::EntityScope &s, std::optional<T> &dst, const picojson::value &src) {
        T entry;
        if (!sp::json::Load(s, entry, src)) {
            return false;
        }
        dst = entry;
        return true;
    }
    template<typename T>
    inline bool Load(const ecs::EntityScope &s, std::vector<T> &dst, const picojson::value &src) {
        if (src.is<picojson::array>()) {
            for (auto &p : src.get<picojson::array>()) {
                auto &entry = dst.emplace_back();
                if (!sp::json::Load(s, entry, p)) {
                    dst.clear();
                    return false;
                }
            }
            return true;
        } else {
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
        robin_hood::unordered_node_map<std::string, T> &dst,
        const picojson::value &src) {
        if (!src.is<picojson::object>()) return false;
        for (auto &p : src.get<picojson::object>()) {
            if (!sp::json::Load(s, dst[p.first], p.second)) {
                dst.clear();
                return false;
            }
        }
        return true;
    }

    // Default Save handler for enums, and all integer and float types
    template<typename T>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const T &src) {
        if constexpr (std::is_enum<T>()) {
            dst = picojson::value(std::string(magic_enum::enum_flags_name(src)));
        } else if constexpr (std::is_convertible_v<T, double>) {
            dst = picojson::value((double)src);
        } else {
            auto &metadata = ecs::StructMetadata::Get<T>();
            static const T defaultValue = {};
            for (auto &field : metadata.fields) {
                field.Save(s, dst, &src, &defaultValue);
            }
            ecs::StructMetadata::Save(s, dst, src);
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
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const glm::vec2 &src) {
        detail::SaveVec<2>(dst, src);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const glm::vec3 &src) {
        detail::SaveVec<3>(dst, src);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const glm::vec4 &src) {
        detail::SaveVec<4>(dst, src);
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
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const glm::ivec2 &src) {
        detail::SaveVec<2>(dst, src);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const glm::ivec3 &src) {
        detail::SaveVec<3>(dst, src);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const glm::quat &src) {
        glm::vec4 r(glm::degrees(glm::angle(src)), glm::axis(src));
        detail::SaveVec<4>(dst, r);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const std::string &src) {
        dst = picojson::value(src);
    }
    template<>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const ecs::EntityRef &src) {
        auto refName = src.Name().String();
        if (!refName.empty() && !src) {
            Errorf("Can't serialize unnamed EntityRef: %s / %s",
                std::to_string(src.GetLive()),
                std::to_string(src.GetStaging()));
            return;
        }
        auto prefix = s.prefix.String();
        size_t prefixLen = 0;
        for (; prefixLen < refName.length() && prefixLen < prefix.length(); prefixLen++) {
            if (refName[prefixLen] != prefix[prefixLen]) break;
        }
        dst = picojson::value(prefixLen >= refName.length() ? refName : refName.substr(prefixLen));
    }
    template<typename T>
    inline void Save(const ecs::EntityScope &s, picojson::value &dst, const std::optional<T> &src) {
        if (src) Save(s, dst, *src);
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
        const robin_hood::unordered_node_map<std::string, T> &src) {
        picojson::object obj = {};
        for (auto &[key, value] : src) {
            Save(s, obj[key], value);
        }
        dst = picojson::value(obj);
    }

    template<typename T>
    inline bool SaveIfChanged(const ecs::EntityScope &s,
        picojson::object &dst,
        const char *field,
        const T &src,
        const T &def) {
        if (src != def) {
            Save(s, dst[field], src);
            return true;
        }
        return false;
    }
} // namespace sp::json

// Defined in components/Transform.cc
namespace sp::json {
    template<>
    bool Load(const ecs::EntityScope &scope, ecs::Transform &dst, const picojson::value &src);
    template<>
    void Save(const ecs::EntityScope &scope, picojson::value &dst, const ecs::Transform &src);
} // namespace sp::json
