#pragma once

#include "core/Common.hh"
#include "core/Logging.hh"

#include <glm/glm.hpp>
#include <picojson/picojson.h>
#include <string>
#include <vector>

namespace sp::json {
    template<typename T>
    inline bool Load(T &dst, const picojson::value &src) {
        if (!src.is<double>()) return false;
        dst = (T)src.get<double>();
        return true;
    }

    template<>
    inline bool Load(bool &dst, const picojson::value &src) {
        if (!src.is<bool>()) return false;
        dst = src.get<bool>();
        return true;
    }

    template<>
    inline bool Load(std::string &dst, const picojson::value &src) {
        if (!src.is<std::string>()) return false;
        dst = src.get<std::string>();
        return true;
    }

    template<size_t N, typename T, glm::precision P>
    inline bool Load(glm::vec<N, T, P> &dst, const picojson::value &src) {
        if (!src.is<picojson::array>()) return false;
        auto &values = src.get<picojson::array>();
        if (values.size() != N) {
            Errorf("Incorrect array size: %u, expected %u", values.size(), N);
            dst = {};
            return false;
        }

        for (size_t i = 0; i < values.size(); i++) {
            if (!values[i].is<double>()) {
                Errorf("Unexpected array value at index %u: %s", i, values[i].to_str());
                dst = {};
                return false;
            }
            dst[i] = (float)values[i].get<double>();
        }
        return true;
    }

    template<typename T>
    inline void Save(picojson::value &dst, const T &src) {
        dst = picojson::value((double)src);
    }

    template<>
    inline void Save(picojson::value &dst, const bool &src) {
        dst = picojson::value(src);
    }

    template<>
    inline void Save(picojson::value &dst, const std::string &src) {
        dst = picojson::value(src);
    }

    template<size_t N, typename T, glm::precision P>
    inline void Save(picojson::value &dst, const glm::vec<N, T, P> &src) {
        picojson::array vec(src.length());

        for (size_t i = 0; i < src.length(); i++) {
            vec[i] = picojson::value((double)src[i]);
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

    /**
     * Returns true if all the specified parameters are present
     */
    bool ParametersExist(const picojson::value &json, std::vector<std::string> reqParams);
} // namespace sp::json
