/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "EditorSystem.hh"

#include "common/Tracing.hh"
#include "console/Console.hh"
#include "ecs/EcsImpl.hh"
#include "game/GameEntities.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"

namespace sp {
    static CVar<float> CVarEditorAngle("e.EditorAngle", -20.0f, "Tilt angle of the entity inspector gui");
    static CVar<float> CVarEditorDistance("e.EditorDistance",
        0.8f,
        "Distance to space the inspector gui from the player");
    static CVar<float> CVarEditorOffset("e.EditorOffset", 0.8f, "Distance to offset the inspector gui from the ground");

    EditorSystem::EditorSystem() {
        funcs.Register<std::string>("edit",
            "Edit the specified entity, or the entity being looked at",
            [this](std::string targetName) {
                OpenEditor(targetName, true);
            });

        funcs.Register<std::string>("editinworld",
            "Edit the specified entity, or the entity being looked at",
            [this](std::string targetName) {
                OpenEditor(targetName, false);
            });

        funcs.Register<void>("tray", "Open or close the model tray", [this]() {
            ToggleTray();
        });

        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "editor",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto inspector = scene->NewSystemEntity(lock, scene, inspectorEntity.Name());
                inspector.Set<ecs::Gui>(lock, "inspector", ecs::GuiTarget::None);
                inspector.Set<ecs::Screen>(lock);
                inspector.Set<ecs::EventInput>(lock);
                auto &transform = inspector.Set<ecs::TransformTree>(lock);
                transform.pose.SetScale(glm::vec3(0.8, 1, 1));

                auto &ph = inspector.Set<ecs::Physics>(lock);
                ph.group = ecs::PhysicsGroup::UserInterface;
                ph.type = ecs::PhysicsActorType::Static;
            });
    }

    void EditorSystem::OpenEditor(std::string targetName, bool flatMode) {
        auto lock = ecs::StartTransaction<ecs::ReadAll,
            ecs::SendEventsLock,
            ecs::Write<ecs::Gui, ecs::FocusLock, ecs::TransformTree, ecs::Physics>>();

        auto inspector = inspectorEntity.Get(lock);

        if (!inspector.Has<ecs::TransformTree, ecs::Gui, ecs::Physics>(lock)) return;

        ecs::Entity target;
        if (targetName.empty()) {
            auto flatview = entities::Flatview.Get(lock);
            if (flatview.Has<ecs::PhysicsQuery>(lock)) {
                auto &query = flatview.Get<ecs::PhysicsQuery>(lock);
                for (auto &subQuery : query.queries) {
                    auto *raycastQuery = std::get_if<ecs::PhysicsQuery::Raycast>(&subQuery);
                    if (raycastQuery && raycastQuery->result) {
                        target = raycastQuery->result->subTarget;
                        if (target) break;
                    }
                }
            }
        } else {
            ecs::EntityRef ref = ecs::Name(targetName, ecs::Name());
            target = ref.Get(lock);
        }

        auto &gui = inspector.Get<ecs::Gui>(lock);
        auto &physics = inspector.Get<ecs::Physics>(lock);
        auto &focusLock = lock.Get<ecs::FocusLock>();

        bool shouldClose = gui.target != ecs::GuiTarget::None && (!target || target == previousTarget);
        previousTarget = target;
        if (shouldClose) {
            if (gui.target == ecs::GuiTarget::Debug) focusLock.ReleaseFocus(ecs::FocusLayer::Overlay);
            gui.target = ecs::GuiTarget::None;
            physics.shapes.clear();
            return;
        }

        ecs::EventBindings::SendEvent(lock, inspectorEntity, ecs::Event{EDITOR_EVENT_EDIT_TARGET, inspector, target});

        if (flatMode) {
            if (gui.target != ecs::GuiTarget::Debug) focusLock.AcquireFocus(ecs::FocusLayer::Overlay);
            gui.target = ecs::GuiTarget::Debug;
            physics.shapes.clear();
        } else {
            if (gui.target == ecs::GuiTarget::Debug) focusLock.ReleaseFocus(ecs::FocusLayer::Overlay);
            gui.target = ecs::GuiTarget::World;
            physics.shapes = {ecs::PhysicsShape::Box(glm::vec3(1, 1, 0.01))};

            auto &transform = inspector.Get<ecs::TransformTree>(lock);

            auto player = entities::Player.Get(lock);
            if (!player.Has<ecs::TransformSnapshot>(lock)) return;

            if (target.Has<ecs::TransformSnapshot>(lock)) {
                auto targetPos = target.Get<const ecs::TransformSnapshot>(lock).globalPose.GetPosition();
                auto playerPos = player.Get<const ecs::TransformSnapshot>(lock).globalPose.GetPosition();
                auto targetDelta = glm::vec3(targetPos.x - playerPos.x, 0, targetPos.z - playerPos.z);
                if (targetDelta != glm::vec3(0)) {
                    auto targetDir = glm::normalize(targetDelta);
                    transform.pose.SetPosition(
                        playerPos + targetDir * CVarEditorDistance.Get() + glm::vec3(0, CVarEditorOffset.Get(), 0));
                    transform.pose.SetRotation(glm::quat(
                        glm::vec3(glm::radians(CVarEditorAngle.Get()), glm::atan(-targetDir.x, -targetDir.z), 0)));
                } else {
                    transform.pose = ecs::Transform(glm::vec3(0, 1, -1));
                }
                transform.parent = {};
            } else {
                transform.pose = ecs::Transform(glm::vec3(0, 1, -1));
                transform.parent = entities::Player;
            }
        }
    }

    void EditorSystem::ToggleTray() {
        bool trayOpen = false;
        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::TransformSnapshot>>();
            ecs::EntityRef trayRef = ecs::Name("tray", "root");
            if (trayRef.Get(lock).Exists(lock)) trayOpen = true;
        }

        if (trayOpen) {
            GetSceneManager().QueueAction(SceneAction::RemoveScene, "tray");
        } else {
            GetSceneManager().QueueAction(SceneAction::AddScene, "tray");
        }
    }
} // namespace sp
