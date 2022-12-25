#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"

namespace sp::scripts {
    using namespace ecs;

    struct ArticulatingArm {
        bool locked = false;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<ecs::Name, ecs::SignalBindings>(lock)) return;

            float lockedRatio = SignalBindings::GetSignal(lock, ent, "locked_ratio");

            bool shouldLock = lockedRatio > 0.999;
            if (shouldLock == locked) return;

            auto rootName = ent.Get<ecs::Name>(lock);

            struct ArmJoint {
                int angularDampingMultiplier;
                EntityRef entity;
            };

            std::array jointNodes = {
                ArmJoint{1, ecs::Name{rootName.scene, rootName.entity + ".Arm_0"}},
                ArmJoint{1, ecs::Name{rootName.scene, rootName.entity + ".Arm_1"}},
                ArmJoint{1, ecs::Name{rootName.scene, rootName.entity + ".Ball_1"}},
                ArmJoint{1, ecs::Name{rootName.scene, rootName.entity + ".Socket_0"}},
                ArmJoint{1, ecs::Name{rootName.scene, rootName.entity + ".Shaft"}},
                ArmJoint{1, ecs::Name{rootName.scene, rootName.entity + ".Knob"}},
                ArmJoint{1, ecs::Name{rootName.scene, rootName.entity + ".Socket_1"}},
            };

            for (auto &node : jointNodes) {
                auto child = node.entity.Get(lock);
                if (!child.Has<ecs::Physics, ecs::PhysicsJoints>(lock)) {
                    continue;
                }

                auto &ph = child.Get<ecs::Physics>(lock);
                ph.angularDamping = lockedRatio * 100 * node.angularDampingMultiplier;
                ph.linearDamping = lockedRatio * 100;

                auto &joints = child.Get<ecs::PhysicsJoints>(lock).joints;
                if (lockedRatio > 0.999) {
                    auto &joint = joints.emplace_back();
                    joint.target = ent;
                    joint.type = ecs::PhysicsJointType::Fixed;
                    joint.localOffset = child.Get<ecs::TransformSnapshot>(lock).GetInverse() *
                                        ent.Get<ecs::TransformSnapshot>(lock);
                } else if (locked) {
                    sp::erase_if(joints, [ent](auto &&arg) {
                        return arg.target == ent && arg.type == ecs::PhysicsJointType::Fixed;
                    });
                }
            }

            locked = shouldLock;
        }
    };
    StructMetadata MetadataArticulatingArm(typeid(ArticulatingArm));
    InternalScript<ArticulatingArm> articulatingArm("articulating_arm", MetadataArticulatingArm);
} // namespace sp::scripts
