#include "AssetHelpers.hh"

#include <glm/glm.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace sp {
    void MakeVec(size_t N, picojson::value val, float *ret) {
        auto values = val.get<picojson::array>();
        Assert(values.size() == N, "incorrect array size");

        for (size_t i = 0; i < values.size(); i++) {
            double v = values[i].get<double>();
            ret[i] = v;
        }
    }

    glm::vec2 MakeVec2(picojson::value src) {
        glm::vec2 ret;
        MakeVec(2, src, (float *)&ret);
        return ret;
    }

    glm::vec3 MakeVec3(picojson::value src) {
        glm::vec3 ret;
        MakeVec(3, src, (float *)&ret);
        return ret;
    }

    glm::vec4 MakeVec4(picojson::value src) {
        glm::vec4 ret;
        MakeVec(4, src, (float *)&ret);
        return ret;
    }

    picojson::value MakeVec2(glm::vec2 src) {
        picojson::array ret(2);
        for (size_t i = 0; i < ret.size(); i++) {
            ret[i] = picojson::value(src[i]);
        }
        return picojson::value(ret);
    }

    picojson::value MakeVec3(glm::vec3 src) {
        picojson::array ret(3);
        for (size_t i = 0; i < ret.size(); i++) {
            ret[i] = picojson::value(src[i]);
        }
        return picojson::value(ret);
    }

    picojson::value MakeVec4(glm::vec4 src) {
        picojson::array ret(4);
        for (size_t i = 0; i < ret.size(); i++) {
            ret[i] = picojson::value(src[i]);
        }
        return picojson::value(ret);
    }

    bool ParametersExist(const picojson::value &json, vector<string> reqParams) {
        vector<bool> found(reqParams.size(), false);

        for (auto param : json.get<picojson::object>()) {
            for (uint32 i = 0; i < found.size(); ++i) {
                if (param.first == reqParams.at(i)) {
                    found.at(i) = true;
                    break;
                }
            }
        }

        for (uint32 i = 0; i < found.size(); ++i) {
            if (!found.at(i)) { return false; }
        }
        return true;
    }
} // namespace sp
