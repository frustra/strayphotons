#pragma once

#include <glm/glm.hpp>
#include <picojson/picojson.h>
#include <string>
#include <vector>

namespace sp {
    glm::vec2 MakeVec2(picojson::value val);
    glm::vec3 MakeVec3(picojson::value val);
    glm::vec4 MakeVec4(picojson::value val);

    /**
     * Returns true if all the specified parameters are present
     */
    bool ParametersExist(const picojson::value &json, std::vector<std::string> reqParams);
} // namespace sp
