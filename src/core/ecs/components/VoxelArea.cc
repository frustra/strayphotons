#include "VoxelArea.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<VoxelArea>::Load(ScenePtr scenePtr,
        const Name &scope,
        VoxelArea &voxelArea,
        const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "extents") {
                if (param.second.is<double>()) {
                    voxelArea.extents = glm::ivec3((int)param.second.get<double>());
                } else if (param.second.is<picojson::array>()) {
                    voxelArea.extents = glm::ivec3(sp::MakeVec3(param.second));
                }
            }
        }
        return true;
    }
} // namespace ecs
