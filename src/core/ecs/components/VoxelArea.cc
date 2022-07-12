#include "VoxelArea.hh"

#include "assets/JsonHelpers.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

namespace ecs {
    template<>
    bool Component<VoxelArea>::Load(const EntityScope &scope, VoxelArea &voxelArea, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "extents") {
                if (param.second.is<double>()) {
                    voxelArea.extents = glm::ivec3((int)param.second.get<double>());
                } else if (param.second.is<picojson::array>()) {
                    if (!sp::json::Load(scope, voxelArea.extents, param.second)) {
                        Errorf("Invalid voxel_area extents: %s", param.second.to_str());
                        return false;
                    }
                }
            }
        }
        return true;
    }
} // namespace ecs
