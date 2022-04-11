#include "PhysicsJoints.hh"

#include "game/Scene.hh"

#include <assets/AssetHelpers.hh>
#include <assets/AssetManager.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<PhysicsJoints>::Load(const EntityScope &scope, PhysicsJoints &joints, const picojson::value &src) {
        auto scene = scope.scene.lock();
        Assert(scene, "PhysicsJoints::Load must have valid scene to define joint");
        for (auto param : src.get<picojson::array>()) {
            string jointTargetName;
            PhysicsJoint joint;

            Transform localTransform, remoteTransform;
            joint.type = PhysicsJointType::Fixed;

            for (auto jointParam : param.get<picojson::object>()) {
                if (jointParam.first == "target") {
                    jointTargetName = jointParam.second.get<string>();
                    joint.target = scene->GetStagingEntity(jointTargetName, scope.prefix);
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
                    joint.range = sp::MakeVec2(jointParam.second);
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
            if (joint.target) {
                joint.localOffset = localTransform.GetPosition();
                joint.localOrient = localTransform.GetRotation();
                joint.remoteOffset = remoteTransform.GetPosition();
                joint.remoteOrient = remoteTransform.GetRotation();
                joints.Add(joint);
            } else {
                Errorf("Component<PhysicsJoints>::Load joint name does not exist: %s", jointTargetName);
                return false;
            }
        }
        return true;
    }

    template<>
    void Component<PhysicsJoints>::ApplyComponent(Lock<ReadAll> srcLock,
        Entity src,
        Lock<AddRemove> dstLock,
        Entity dst) {
        if (src.Has<PhysicsJoints>(srcLock) && !dst.Has<PhysicsJoints>(dstLock)) {
            auto &joints = dst.Set<PhysicsJoints>(dstLock, src.Get<PhysicsJoints>(srcLock));

            // Map physics joints from staging id to live id
            for (auto &joint : joints.joints) {
                if (joint.target && joint.target.Has<SceneInfo>(srcLock)) {
                    auto &sceneInfo = joint.target.Get<SceneInfo>(srcLock);
                    joint.target = sceneInfo.liveId;
                }
            }
        }
    }
} // namespace ecs
