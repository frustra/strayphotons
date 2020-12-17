#pragma once

#include <Common.hh>
#include <glm/glm.hpp>
#include <picojson/picojson.h>

namespace sp {
    glm::vec2 MakeVec2(picojson::value src);
    glm::vec3 MakeVec3(picojson::value src);
    glm::vec4 MakeVec4(picojson::value src);

    picojson::value MakeVec2(glm::vec2 src);
    picojson::value MakeVec3(glm::vec3 src);
    picojson::value MakeVec4(glm::vec4 src);

    /**
     * Returns true if all the specified parameters are present
     */
    bool ParametersExist(const picojson::value &json, vector<string> reqParams);
} // namespace sp
