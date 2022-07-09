#include "PhysicsJoints.hh"

#include "assets/AssetManager.hh"
#include "assets/JsonHelpers.hh"
#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    template<>
    bool Component<PhysicsJoints>::Load(const EntityScope &scope, PhysicsJoints &joints, const picojson::value &src) {
        auto scene = scope.scene.lock();
        Assert(scene, "PhysicsJoints::Load must have valid scene to define joint");
        for (auto param : src.get<picojson::array>()) {
            Name jointTargetName;
            PhysicsJoint joint;

            Transform localTransform, remoteTransform;
            joint.type = PhysicsJointType::Fixed;

            for (auto jointParam : param.get<picojson::object>()) {
                if (jointParam.first == "target") {
                    jointTargetName = Name(jointParam.second.get<string>(), scope.prefix);
                } else if (jointParam.first == "type") {
                    auto typeString = jointParam.second.get<string>();
                    sp::to_upper(typeString);
                    if (typeString == "FIXED") {
                        joint.type = PhysicsJointType::Fixed;
                    } else if (typeString == "DISTANCE") {
                        joint.type = PhysicsJointType::Distance;
                    } else if (typeString == "SPHERICAL") {
                        joint.type = PhysicsJointType::Spherical;
                    } else if (typeString == "HINGE") {
                        joint.type = PhysicsJointType::Hinge;
                    } else if (typeString == "SLIDER") {
                        joint.type = PhysicsJointType::Slider;
                    } else {
                        Errorf("Unknown joint type: %s", typeString);
                        return false;
                    }
                } else if (jointParam.first == "range") {
                    if (!sp::json::Load(joint.range, jointParam.second)) {
                        Errorf("Invalid physics_joint range: %s", jointParam.second.to_str());
                        return false;
                    }
                } else if (jointParam.first == "local_offset") {
                    if (!Component<Transform>::Load(scope, localTransform, jointParam.second)) {
                        Errorf("Couldn't parse physics joint local_offset as Transform");
                        return false;
                    }
                } else if (jointParam.first == "remote_offset") {
                    if (!Component<Transform>::Load(scope, remoteTransform, jointParam.second)) {
                        Errorf("Couldn't parse physics joint remote_offset as Transform");
                        return false;
                    }
                }
            }
            if (jointTargetName) {
                joint.target = jointTargetName;
                joint.localOffset = localTransform.GetPosition();
                joint.localOrient = localTransform.GetRotation();
                joint.remoteOffset = remoteTransform.GetPosition();
                joint.remoteOrient = remoteTransform.GetRotation();
                joints.Add(joint);
            } else {
                Errorf("Component<PhysicsJoints>::Load joint name does not exist: %s", jointTargetName.String());
                return false;
            }
        }
        return true;
    }

    template<>
    void Component<PhysicsJoints>::Apply(const PhysicsJoints &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstJoints = dst.Get<PhysicsJoints>(lock);
        for (auto &joint : src.joints) {
            if (!sp::contains(dstJoints.joints, joint)) dstJoints.joints.emplace_back(joint);
        }
    }
} // namespace ecs
