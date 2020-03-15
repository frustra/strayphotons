#include "ecs/components/TriggerArea.hh"

#include <ecs/Components.hh>
#include <picojson/picojson.h>
#include <assets/AssetHelpers.hh>

namespace ecs
{
	template<>
	bool Component<TriggerArea>::LoadEntity(Entity &dst, picojson::value &src)
	{
		auto area = dst.Assign<TriggerArea>();
		for (auto param : src.get<picojson::object>())
		{
			if (param.first == "min")
			{
				area->boundsMin = sp::MakeVec3(param.second);
			}
			else if (param.first == "max")
			{
				area->boundsMax = sp::MakeVec3(param.second);
			}
			else if (param.first == "command")
			{
				area->command = param.second.get<string>();
			}
		}
		return true;
	}
}
