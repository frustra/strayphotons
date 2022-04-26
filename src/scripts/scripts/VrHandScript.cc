#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    InternalScript vrHandScript("vr_hand",
        [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
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
                Abortf("Invalid hand specified for VrHand script: %s", handStr);
            }

            auto scene = state.scope.scene.lock();
            Assertf(scene, "VrHand script does not have a valid scene: %s", ToString(lock, ent));

            std::array<std::string, 5> fingerNames = {"thumb", "index", "middle", "ring", "pinky"};
            std::array<std::string, 5> fingerSegmentNames = {
                "meta_" + handStr,
                "0_" + handStr,
                "1_" + handStr,
                "2_" + handStr,
                handStr + "_end",
            };

            for (auto &fingerName : fingerNames) {
                for (auto &segmentName : fingerSegmentNames) {
                    std::string boneName = "finger_" + fingerName + "_" + segmentName;
                    ecs::Name inputName(boneName, inputScope);

                    EntityRef inputEntity(inputName);
                    if (!inputEntity) {
                        Errorf("VrHand script has invalid input entity: %s", inputName.String());
                        continue;
                    }

                    // Physics physics;
                    // physics.shapes.emplace_back(PhysicsShape::Capsule(0.5f, 0.01f));

                    // physics.constraint = inputEntity;
                    // physics.group = PhysicsGroup::Player;
                    // Component<Physics>::Apply(physics, lock, physicsEnt);
                }
            }

            {
                ecs::Name inputName("wrist_" + handStr, inputScope);

                EntityRef inputEntity(inputName);
                if (!inputEntity) {
                    Errorf("VrHand script has invalid input entity: %s", inputName.String());
                    return;
                }

                // Physics physics;
                // auto &boxShape = physics.shapes.emplace_back(PhysicsShape::Box(glm::vec3(0.04, 0.095, 0.11)));
                // boxShape.transform.Translate(glm::vec3(0.005, 0.01, 0.03));

                // physics.constraint = inputEntity;
                // physics.group = PhysicsGroup::PlayerHands;
                // Component<Physics>::Apply(physics, lock, physicsEnt);
            }
        });
} // namespace ecs
