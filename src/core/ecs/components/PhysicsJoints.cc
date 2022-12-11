#include "PhysicsJoints.hh"

#include "assets/AssetManager.hh"
#include "assets/JsonHelpers.hh"
#include "core/Common.hh"
#include "ecs/EcsImpl.hh"

namespace ecs {
    template<>
    void Component<PhysicsJoints>::Apply(PhysicsJoints &dst, const PhysicsJoints &src, bool liveTarget) {
        for (auto &joint : src.joints) {
            if (!sp::contains(dst.joints, joint)) dst.joints.emplace_back(joint);
        }
    }
} // namespace ecs
