#include "AssetHelpers.hh"

#include "core/Common.hh"

#include <string>
#include <vector>

namespace sp {
    void MakeVec(size_t N, picojson::value val, float *ret) {
        auto values = val.get<picojson::array>();
        Assertf(values.size() == N, "Incorrect array size: %u, expected %u", values.size(), N);

        for (size_t i = 0; i < values.size(); i++) {
            ret[i] = (float)values[i].get<double>();
        }
    }

    glm::vec2 MakeVec2(picojson::value val) {
        glm::vec2 ret;
        MakeVec(2, val, (float *)&ret);
        return ret;
    }

    glm::vec3 MakeVec3(picojson::value val) {
        glm::vec3 ret;
        MakeVec(3, val, (float *)&ret);
        return ret;
    }

    glm::vec4 MakeVec4(picojson::value val) {
        glm::vec4 ret;
        MakeVec(4, val, (float *)&ret);
        return ret;
    }

    bool ParametersExist(const picojson::value &json, std::vector<std::string> reqParams) {
        std::vector<bool> found(reqParams.size(), false);

        for (auto param : json.get<picojson::object>()) {
            for (uint32 i = 0; i < found.size(); ++i) {
                if (param.first == reqParams.at(i)) {
                    found.at(i) = true;
                    break;
                }
            }
        }

        for (uint32_t i = 0; i < found.size(); ++i) {
            if (!found.at(i)) { return false; }
        }
        return true;
    }
} // namespace sp
