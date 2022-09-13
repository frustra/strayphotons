#include "EditorSystem.hh"

#include "console/Console.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/GameEntities.hh"
#include "game/SceneManager.hh"

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

        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "editor",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto inspector = scene->NewSystemEntity(lock, scene, inspectorEntity.Name());
                inspector.Set<ecs::Gui>(lock, "inspector", ecs::GuiTarget::None);
                inspector.Set<ecs::Screen>(lock);
                inspector.Set<ecs::EventInput>(lock,
                    INTERACT_EVENT_INTERACT_POINT,
                    INTERACT_EVENT_INTERACT_PRESS,
                    EDITOR_EVENT_EDIT_TARGET);
                inspector.Set<ecs::TransformTree>(lock);
                auto &ph = inspector.Set<ecs::Physics>(lock);
                ph.group = ecs::PhysicsGroup::UserInterface;
                ph.dynamic = false;
            });
    }

    void EditorSystem::OpenEditor(std::string targetName, bool flatMode) {
        auto lock = ecs::StartTransaction<ecs::ReadAll,
            ecs::SendEventsLock,
            ecs::Write<ecs::Gui, ecs::TransformTree, ecs::Physics>>();

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
                        target = raycastQuery->result->target;
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

        if (!target.Exists(lock)) {
            gui.target = ecs::GuiTarget::None;
            physics.shapes.clear();
            return;
        }

        ecs::EventBindings::SendEvent(lock, "/edit/target", inspector, target);

        if (flatMode) {
            gui.target = ecs::GuiTarget::Debug;
            physics.shapes.clear();
        } else {
            gui.target = ecs::GuiTarget::World;
            physics.shapes = {ecs::PhysicsShape::Box(glm::vec3(1, 1, 0.01))};

            auto &transform = inspector.Get<ecs::TransformTree>(lock);

            auto player = entities::Player.Get(lock);
            if (!player.Has<ecs::TransformSnapshot>(lock)) return;

            if (target.Has<ecs::TransformSnapshot>(lock)) {
                auto targetPos = target.Get<const ecs::TransformSnapshot>(lock).GetPosition();
                auto playerPos = player.Get<const ecs::TransformSnapshot>(lock).GetPosition();
                auto targetDir = glm::normalize(glm::vec3(targetPos.x - playerPos.x, 0, targetPos.z - playerPos.z));
                transform.pose.SetPosition(
                    playerPos + targetDir * CVarEditorDistance.Get() + glm::vec3(0, CVarEditorOffset.Get(), 0));
                transform.pose.SetRotation(glm::quat(
                    glm::vec3(glm::radians(CVarEditorAngle.Get()), glm::atan(-targetDir.x, -targetDir.z), 0)));
                transform.parent = {};
            } else {
                transform.pose = ecs::Transform(glm::vec3(0, 1, -1));
                transform.parent = entities::Player;
            }
        }
    }
} // namespace sp
