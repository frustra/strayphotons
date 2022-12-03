#include "PhysicsJoints.hh"

#include "assets/AssetManager.hh"
#include "assets/JsonHelpers.hh"
#include "core/Common.hh"
#include "ecs/EcsImpl.hh"

namespace ecs {
    template<>
    void Component<PhysicsJoints>::Apply(const PhysicsJoints &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstJoints = dst.Get<PhysicsJoints>(lock);
        for (auto &joint : src.joints) {
            if (!sp::contains(dstJoints.joints, joint)) dstJoints.joints.emplace_back(joint);
        }
    }
} // namespace ecs
