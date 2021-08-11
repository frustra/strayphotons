#include "VoxelArea.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<VoxelArea>::Load(Lock<Read<ecs::Name>> lock, VoxelArea &voxelArea, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "min") {
                voxelArea.min = sp::MakeVec3(param.second);
            } else if (param.first == "max") {
                voxelArea.max = sp::MakeVec3(param.second);
            }
        }
        return true;
    }
} // namespace ecs
