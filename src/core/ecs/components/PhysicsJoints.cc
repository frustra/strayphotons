#include "PhysicsJoints.hh"

#include "assets/AssetManager.hh"
#include "assets/JsonHelpers.hh"
#include "core/Common.hh"
#include "ecs/EcsImpl.hh"

namespace ecs {
    template<>
    void StructMetadata::Save<PhysicsJoints>(const EntityScope &scope,
        picojson::value &dst,
        const PhysicsJoints &src,
        const PhysicsJoints &def) {
        picojson::array arrayOut;
        for (auto &joint : src.joints) {
            if (sp::contains(def.joints, joint)) continue;

            picojson::value val;
            sp::json::Save(scope, val, joint);
            arrayOut.emplace_back(val);
        }
        if (arrayOut.size() > 1) {
            dst = picojson::value(arrayOut);
        } else if (arrayOut.size() == 1) {
            dst = arrayOut.front();
        } else {
            dst.set<picojson::array>({});
        }
    }

    template<>
    void Component<PhysicsJoints>::Apply(PhysicsJoints &dst, const PhysicsJoints &src, bool liveTarget) {
        for (auto &joint : src.joints) {
            if (!sp::contains(dst.joints, joint)) dst.joints.emplace_back(joint);
        }
    }
} // namespace ecs
