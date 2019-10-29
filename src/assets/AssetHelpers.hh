#pragma once

#include "Common.hh"
#include <glm/glm.hpp>
#include <tinygltfloader/picojson.h>

namespace sp
{
	glm::vec2 MakeVec2(picojson::value val);
	glm::vec3 MakeVec3(picojson::value val);
	glm::vec4 MakeVec4(picojson::value val);

	/**
	 * Returns true if all the specified parameters are present
	 */
	bool ParametersExist(picojson::value &json, vector<string> reqParams);
}