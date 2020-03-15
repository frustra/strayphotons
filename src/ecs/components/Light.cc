#include "ecs/components/Light.hh"

#include <picojson/picojson.h>
#include <assets/AssetHelpers.hh>

namespace ecs
{
	template<>
	bool Component<Light>::LoadEntity(Entity &dst, picojson::value &src)
	{
		auto light = dst.Assign<ecs::Light>();
		for (auto param : src.get<picojson::object>())
		{
			if (param.first == "intensity")
			{
				light->intensity = param.second.get<double>();
			}
			else if (param.first == "illuminance")
			{
				light->illuminance = param.second.get<double>();
			}
			else if (param.first == "spotAngle")
			{
				light->spotAngle = glm::radians(param.second.get<double>());
			}
			else if (param.first == "tint")
			{
				light->tint = sp::MakeVec3(param.second);
			}
			else if (param.first == "gel")
			{
				light->gelId = param.second.get<bool>() ? 1 : 0;
			}
			else if (param.first == "on")
			{
				light->on = param.second.get<bool>();
			}
			else if (param.first == "bulb")
			{
				light->bulb = param.second.get<string>();
			}
		}
		return true;
	}
}
