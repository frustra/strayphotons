#pragma once

#include "core/Common.hh"
#include "core/Logging.hh"

#include <glm/glm.hpp>
#include <picojson/picojson.h>
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

    // Default Load handler for all integer and float types
    template<typename T>
    inline bool Load(T &dst, const picojson::value &src) {
        if (!src.is<double>()) return false;
        dst = (T)src.get<double>();
        return true;
    }

    // Load() specializations for native and complex types
    template<>
    inline bool Load(bool &dst, const picojson::value &src) {
        if (!src.is<bool>()) return false;
        dst = src.get<bool>();
        return true;
    }
    template<>
    inline bool Load(glm::vec2 &dst, const picojson::value &src) {
        return detail::LoadVec<2>(dst, src);
    }
    template<>
    inline bool Load(glm::ivec2 &dst, const picojson::value &src) {
        return detail::LoadVec<2>(dst, src);
    }
    template<>
    inline bool Load(glm::vec3 &dst, const picojson::value &src) {
        return detail::LoadVec<3>(dst, src);
    }
    template<>
    inline bool Load(glm::ivec3 &dst, const picojson::value &src) {
        return detail::LoadVec<3>(dst, src);
    }
    template<>
    inline bool Load(glm::vec4 &dst, const picojson::value &src) {
        return detail::LoadVec<4>(dst, src);
    }
    template<>
    inline bool Load(std::string &dst, const picojson::value &src) {
        if (!src.is<std::string>()) return false;
        dst = src.get<std::string>();
        return true;
    }
    template<typename T>
    inline bool Load(std::vector<T> &dst, const picojson::value &src) {
        if (!src.is<picojson::array>()) return false;

        for (auto &p : src.get<picojson::array>()) {
            auto &point = dst.emplace_back();
            if (!sp::json::Load(point, p)) return false;
        }
        return true;
    }

    // Default Save handler for all integer and float types
    template<typename T>
    inline void Save(picojson::value &dst, const T &src) {
        dst = picojson::value((double)src);
    }

    // Save() specializations for native and complex types
    template<>
    inline void Save(picojson::value &dst, const bool &src) {
        dst = picojson::value(src);
    }
    template<>
    inline void Save(picojson::value &dst, const glm::vec2 &src) {
        return detail::SaveVec<2>(dst, src);
    }
    template<>
    inline void Save(picojson::value &dst, const glm::vec3 &src) {
        return detail::SaveVec<3>(dst, src);
    }
    template<>
    inline void Save(picojson::value &dst, const glm::vec4 &src) {
        return detail::SaveVec<4>(dst, src);
    }
    template<>
    inline void Save(picojson::value &dst, const std::string &src) {
        dst = picojson::value(src);
    }
    template<typename T>
    inline void Save(picojson::value &dst, const std::vector<T> &src) {
        picojson::array vec(src.size());
        for (size_t i = 0; i < src.size(); i++) {
            Save(vec[i], src[i]);
        }
        dst = picojson::value(vec);
    }

    template<typename T>
    inline bool SaveIfChanged(picojson::object &dst, const char *field, const T &src, const T &def) {
        if (src != def) {
            Save(dst[field], src);
            return true;
        }
        return false;
    }
} // namespace sp::json
