#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    InternalPrefab vrHandPrefab("vr_hand", [](ScriptState &state, Lock<AddRemove> lock, Entity ent) {
        auto handStr = state.GetParam<std::string>("hand");
        sp::to_lower(handStr);
        ecs::Name inputScope("input", "");
        if (handStr == "left") {
            handStr = "l";
            inputScope.entity = "vr_actions_main_in_lefthand_anim";
        } else if (handStr == "right") {
            handStr = "r";
            inputScope.entity = "vr_actions_main_in_righthand_anim";
        } else {
            Errorf("Invalid hand specified for VrHand prefab: %s", handStr);
            return;
        }

        auto scene = state.scope.scene.lock();
        Assertf(scene, "VrHand prefab does not have a valid scene: %s", ToString(lock, ent));

        auto prefabScope = state.scope.prefix;
        if (ent.Has<Name>(lock)) prefabScope = ent.Get<Name>(lock);

        std::array<std::string, 5> fingerNames = {"thumb", "index", "middle", "ring", "pinky"};
        std::array<std::string, 4> fingerSegmentNames = {
            "meta_" + handStr,
            "0_" + handStr,
            "1_" + handStr,
            "2_" + handStr,
        };

        for (auto &fingerName : fingerNames) {
            {
                std::string boneName = "finger_" + fingerName + "_" + handStr + "_end";
                ecs::Name physicsName(boneName, prefabScope);
                ecs::Name inputName(boneName, inputScope);

                auto physicsEnt = scene->GetStagingEntity(physicsName);
                if (!physicsEnt) {
                    Errorf("VrHand prefab could not find physics entity: %s", physicsName.String());
                    continue;
                }
                EntityRef inputEntity(inputName);
                if (!inputEntity) {
                    Errorf("VrHand prefab has invalid input entity: %s", inputName.String());
                    continue;
                }

                Physics physics;
                physics.shapes.emplace_back(PhysicsShape::Sphere(0.01f));

                physics.constraint = inputEntity;
                physics.group = PhysicsGroup::Player;
                // physics.kinematic = true;
                Component<Physics>::Apply(physics, lock, physicsEnt);
            }

            for (auto &segmentName : fingerSegmentNames) {
                std::string boneName = "finger_" + fingerName + "_" + segmentName;
                ecs::Name physicsName(boneName, prefabScope);
                ecs::Name inputName(boneName, inputScope);

                auto physicsEnt = scene->GetStagingEntity(physicsName);
                if (!physicsEnt) {
                    // Errorf("VrHand prefab could not find physics entity: %s", physicsName.String());
                    continue;
                }
                EntityRef inputEntity(inputName);
                if (!inputEntity) {
                    Errorf("VrHand prefab has invalid input entity: %s", inputName.String());
                    continue;
                }

                // if (physicsEnt.Has<TransformTree>(lock)) {
                //     auto &transform = physicsEnt.Get<ecs::TransformTree>(lock);
                //     transform.parent = inputEntity;
                //     transform.pose = {};
                // }

                Physics physics;
                physics.shapes.emplace_back(PhysicsShape::Capsule(0.5f, 0.01f));

                physics.constraint = inputEntity;
                physics.group = PhysicsGroup::Player;
                // physics.kinematic = true;
                Component<Physics>::Apply(physics, lock, physicsEnt);
            }
        }

        {
            ecs::Name physicsName("wrist_" + handStr, prefabScope);
            ecs::Name inputName("wrist_" + handStr, inputScope);

            auto physicsEnt = scene->GetStagingEntity(physicsName);
            if (!physicsEnt) {
                Errorf("VrHand prefab could not find physics entity: %s", physicsName.String());
                return;
            }
            EntityRef inputEntity(inputName);
            if (!inputEntity) {
                Errorf("VrHand prefab has invalid input entity: %s", inputName.String());
                return;
            }

            Physics physics;
            auto &boxShape = physics.shapes.emplace_back(PhysicsShape::Box(glm::vec3(0.04, 0.095, 0.11)));
            boxShape.transform.Translate(glm::vec3(0.005, 0.01, 0.03));

            physics.constraint = inputEntity;
            physics.group = PhysicsGroup::PlayerHands;
            Component<Physics>::Apply(physics, lock, physicsEnt);
        }
    });
} // namespace ecs
