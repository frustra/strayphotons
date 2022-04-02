#include "core/Common.hh"
#include "ecs/EcsImpl.hh"

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
                ecs::Name name;
            };

            std::array jointNodes = {
                ArmJoint{false, 4, {rootName.scene, rootName.entity + ".Ball_0"}},
                ArmJoint{false, 1, {rootName.scene, rootName.entity + ".Arm_0"}},
                ArmJoint{false, 1, {rootName.scene, rootName.entity + ".Arm_1"}},
                ArmJoint{false, 4, {rootName.scene, rootName.entity + ".Ball_1"}},

                ArmJoint{true, 1, {rootName.scene, rootName.entity + ".Socket_0"}},
                ArmJoint{true, 1, {rootName.scene, rootName.entity + ".Shaft"}},
                ArmJoint{true, 1, {rootName.scene, rootName.entity + ".Socket_1"}},
            };

            ecs::Entity lastNode;

            for (auto &node : jointNodes) {
                auto child = ecs::EntityWith<ecs::Name>(lock, node.name);
                if (!child.Has<ecs::Physics, ecs::PhysicsJoints>(lock)) {
                    lastNode = child;
                    continue;
                }

                auto &ph = child.Get<ecs::Physics>(lock);
                ph.angularDamping = lockedRatio * 100 * node.angularDampingMultiplier;
                ph.linearDamping = lockedRatio * 100;

                if (!node.fixed && lastNode) {
                    auto &joints = child.Get<ecs::PhysicsJoints>(lock).joints;
                    if (lockedRatio > 0.999) {
                        bool createFixed = true;
                        for (auto it = joints.begin(); it != joints.end(); it++) {
                            if (it->type == ecs::PhysicsJointType::Fixed) {
                                createFixed = false;
                                break;
                            }
                        }
                        if (createFixed) {
                            auto lastTrans = lastNode.Get<ecs::TransformSnapshot>(lock);
                            auto thisTrans = child.Get<ecs::TransformSnapshot>(lock);
                            ecs::PhysicsJoint joint = joints.front();
                            joint.localOrient = glm::inverse(thisTrans.GetRotation()) * lastTrans.GetRotation();
                            joint.remoteOrient = {};
                            joint.target = lastNode;
                            joint.type = ecs::PhysicsJointType::Fixed;
                            joints.push_back(joint);
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

                lastNode = child;
            }
        });
} // namespace sp::scripts
