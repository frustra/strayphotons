#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"

namespace sp::scripts {
    using namespace ecs;

    struct ArticulatingArm {
        bool locked = false;

        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            if (!entLock.Has<ecs::Name, ecs::SignalBindings>()) return;

            float lockedRatio = SignalBindings::GetSignal(entLock, "locked_ratio");

            bool shouldLock = lockedRatio > 0.999;
            if (shouldLock == locked) return;

            auto rootName = entLock.Get<ecs::Name>();

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
                auto child = node.entity.Get(entLock);
                if (!child.Has<ecs::Physics, ecs::PhysicsJoints>(entLock)) {
                    continue;
                }

                // TODO: Send an event the entity to change these values
                // auto &ph = child.Get<ecs::Physics>(entLock);
                // ph.angularDamping = lockedRatio * 100 * node.angularDampingMultiplier;
                // ph.linearDamping = lockedRatio * 100;

                // auto &joints = child.Get<ecs::PhysicsJoints>(entLock).joints;
                // if (lockedRatio > 0.999) {
                //     auto &joint = joints.emplace_back();
                //     joint.target = entLock.entity;
                //     joint.type = ecs::PhysicsJointType::Fixed;
                //     joint.localOffset = child.Get<ecs::TransformSnapshot>(entLock).GetInverse() *
                //                         entLock.Get<ecs::TransformSnapshot>();
                // } else if (locked) {
                //     sp::erase_if(joints, [ent = entLock.entity](auto &&arg) {
                //         return arg.target == ent && arg.type == ecs::PhysicsJointType::Fixed;
                //     });
                // }
            }

            locked = shouldLock;
        }
    };
    StructMetadata MetadataArticulatingArm(typeid(ArticulatingArm));
    InternalScript<ArticulatingArm> articulatingArm("articulating_arm", MetadataArticulatingArm);
} // namespace sp::scripts
