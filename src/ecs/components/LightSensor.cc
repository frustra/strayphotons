#include "ecs/components/LightSensor.hh"

#include "ecs/components/SignalReceiver.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<LightSensor>::Load(Lock<Read<ecs::Name>> lock, LightSensor &sensor, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "translate") {
                sensor.position = sp::MakeVec3(param.second);
            } else if (param.first == "direction") {
                sensor.direction = sp::MakeVec3(param.second);
            } else if (param.first == "outputTo") {
                for (auto entName : param.second.get<picojson::array>()) {
                    auto outputName = entName.get<string>();
                    for (auto ent : lock.EntitiesWith<Name>()) {
                        if (ent.Get<Name>(lock) == outputName) {
                            sensor.outputTo.emplace_back(ent);
                            break;
                        }
                    }
                }
            } else if (param.first == "onColor") {
                sensor.onColor = sp::MakeVec3(param.second);
            } else if (param.first == "offColor") {
                sensor.offColor = sp::MakeVec3(param.second);
            } else if (param.first == "triggers") {
                for (auto trigger : param.second.get<picojson::array>()) {
                    ecs::LightSensor::Trigger tr;
                    for (auto param : trigger.get<picojson::object>()) {
                        if (param.first == "illuminance") {
                            tr.illuminance = sp::MakeVec3(param.second);
                        } else if (param.first == "oncmd") {
                            tr.oncmd = param.second.get<string>();
                        } else if (param.first == "offcmd") {
                            tr.offcmd = param.second.get<string>();
                        } else if (param.first == "onSignal") {
                            tr.onSignal = param.second.get<double>();
                        } else if (param.first == "offSignal") {
                            tr.offSignal = param.second.get<double>();
                        }
                    }
                    sensor.triggers.push_back(tr);
                }
            }
        }
        return true;
    }
} // namespace ecs
