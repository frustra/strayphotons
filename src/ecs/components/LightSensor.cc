#include "ecs/components/LightSensor.hh"
#include "ecs/components/SignalReceiver.hh"

#include <ecs/Components.hh>
#include <picojson/picojson.h>
#include <assets/AssetHelpers.hh>

namespace ecs
{
	template<>
	bool Component<LightSensor>::LoadEntity(Entity &dst, picojson::value &src)
	{
		auto sensor = dst.Assign<LightSensor>();
		for (auto param : src.get<picojson::object>())
		{
			if (param.first == "translate")
			{
				sensor->position = sp::MakeVec3(param.second);
			}
			else if (param.first == "direction")
			{
				sensor->direction = sp::MakeVec3(param.second);
			}
			else if (param.first == "outputTo")
			{
				for (auto entName : param.second.get<picojson::array>())
				{
					sensor->outputTo.emplace_back(entName.get<string>(), [dst](NamedEntity & ent)
					{
						if (!ent->Has<SignalReceiver>())
						{
							return false;
						}
						auto receiver = ent->Get<SignalReceiver>();
						receiver->AttachSignal(dst);
						return true;
					});
				}
			}
			else if (param.first == "onColor")
			{
				sensor->onColor = sp::MakeVec3(param.second);
			}
			else if (param.first == "offColor")
			{
				sensor->offColor = sp::MakeVec3(param.second);
			}
			else if (param.first == "triggers")
			{
				for (auto trigger : param.second.get<picojson::array>())
				{
					ecs::LightSensor::Trigger tr;
					for (auto param : trigger.get<picojson::object>())
					{
						if (param.first == "illuminance")
						{
							tr.illuminance = sp::MakeVec3(param.second);
						}
						else if (param.first == "oncmd")
						{
							tr.oncmd = param.second.get<string>();
						}
						else if (param.first == "offcmd")
						{
							tr.offcmd = param.second.get<string>();
						}
						else if (param.first == "onSignal")
						{
							tr.onSignal = param.second.get<double>();
						}
						else if (param.first == "offSignal")
						{
							tr.offSignal = param.second.get<double>();
						}
					}
					sensor->triggers.push_back(tr);
				}
			}
		}
		return true;
	}
}
