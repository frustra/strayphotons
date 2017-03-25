#include "AssetHelpers.hh"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace sp
{
	void MakeVec(size_t N, picojson::value val, float *ret)
	{
		auto values = val.get<picojson::array>();
		Assert(values.size() == N, "incorrect array size");

		for (size_t i = 0; i < values.size(); i++)
		{
			double v = values[i].get<double>();
			ret[i] = v;
		}
	}

	glm::vec2 MakeVec2(picojson::value val)
	{
		glm::vec2 ret;
		MakeVec(2, val, (float *) &ret);
		return ret;
	}

	glm::vec3 MakeVec3(picojson::value val)
	{
		glm::vec3 ret;
		MakeVec(3, val, (float *) &ret);
		return ret;
	}

	glm::vec4 MakeVec4(picojson::value val)
	{
		glm::vec4 ret;
		MakeVec(4, val, (float *) &ret);
		return ret;
	}
}
