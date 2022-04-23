#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"

namespace sp::scripts {
    using namespace ecs;

    InternalScript articulatingArm("articulating_arm",
        [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<ecs::Name, ecs::SignalBindings>(lock)) return;

            float lockedRatio = SignalBindings::GetSignal(lock, ent, "locked_ratio");

            if (state.userData.has_value() && std::any_cast<float>(state.userData) == lockedRatio) return;

            state.userData = lockedRatio;

            auto rootName = ent.Get<ecs::Name>(lock);

            struct ArmJoint {
                bool fixed;
                int angularDampingMultiplier;
                EntityRef entity;
            };

            std::array jointNodes = {
                ArmJoint{false, 1, ecs::Name{rootName.scene, rootName.entity + ".Arm_0"}},
                ArmJoint{false, 1, ecs::Name{rootName.scene, rootName.entity + ".Arm_1"}},
                ArmJoint{false, 1, ecs::Name{rootName.scene, rootName.entity + ".Ball_1"}},

                ArmJoint{true, 1, ecs::Name{rootName.scene, rootName.entity + ".Socket_0"}},
                ArmJoint{true, 1, ecs::Name{rootName.scene, rootName.entity + ".Shaft"}},
                ArmJoint{true, 1, ecs::Name{rootName.scene, rootName.entity + ".Socket_1"}},
            };

            for (auto &node : jointNodes) {
                auto child = node.entity.Get(lock);
                if (!child.Has<ecs::Physics, ecs::PhysicsJoints>(lock)) { continue; }

                auto &ph = child.Get<ecs::Physics>(lock);
                ph.angularDamping = lockedRatio * 100 * node.angularDampingMultiplier;
                ph.linearDamping = lockedRatio * 100;

                if (node.fixed) continue;

                auto &joints = child.Get<ecs::PhysicsJoints>(lock).joints;
                if (lockedRatio > 0.999) {
                    bool createFixed = true;
                    for (auto it = joints.begin(); it != joints.end(); it++) {
                        if (it->type == ecs::PhysicsJointType::Fixed) {
                            createFixed = false;
                            break;
                        }
                    }
                    if (createFixed && !joints.empty()) {
                        ecs::PhysicsJoint joint = joints.front();
                        auto jointTarget = joint.target.Get(lock);
                        if (jointTarget.Has<ecs::TransformSnapshot>(lock)) {
                            auto targetTransform = jointTarget.Get<ecs::TransformSnapshot>(lock);
                            auto thisTransform = child.Get<ecs::TransformSnapshot>(lock);
                            joint.localOrient = glm::inverse(thisTransform.GetRotation()) *
                                                targetTransform.GetRotation();
                            joint.remoteOrient = {};
                            joint.type = ecs::PhysicsJointType::Fixed;
                            joints.push_back(joint);
                        }
                    }
                } else {
                    for (auto it = joints.begin(); it != joints.end(); it++) {
                        if (it->type == ecs::PhysicsJointType::Fixed) {
                            joints.erase(it);
                            break;
                        }
                    }
                }
            }
        });
} // namespace sp::scripts
