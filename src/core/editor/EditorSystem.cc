#include "EditorSystem.hh"

#include "console/Console.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/SceneManager.hh"

namespace sp {
    static CVar<float> CVarEditorAngle("e.EditorAngle", -20.0f, "Tilt angle of the entity inspector gui");
    static CVar<float> CVarEditorDistance("e.EditorDistance",
        0.8f,
        "Distance to space the inspector gui from the player");
    static CVar<float> CVarEditorOffset("e.EditorOffset", 0.8f, "Distance to offset the inspector gui from the ground");

    EditorSystem::EditorSystem() {
        funcs.Register(this,
            "edit",
            "Edit the specified entity, or the entity being looked at",
            &EditorSystem::OpenEditorFlat);

        funcs.Register(this,
            "editinworld",
            "Edit the specified entity, or the entity being looked at",
            &EditorSystem::OpenEditorWorld);

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
                inspector.Set<ecs::Physics>(lock,
                    ecs::PhysicsShape::Box(glm::vec3(1, 1, 0.01)),
                    ecs::PhysicsGroup::NoClip,
                    false /* dynamic */);
                inspector.Set<ecs::TransformTree>(lock);
            });
    }

    EditorSystem::~EditorSystem() {
        GetSceneManager().QueueActionAndBlock(SceneAction::RemoveScene, "editor");
    }

    void EditorSystem::OpenEditorFlat(std::string targetName) {
        OpenEditor(targetName, true);
    }

    void EditorSystem::OpenEditorWorld(std::string targetName) {
        OpenEditor(targetName, false);
    }

    void EditorSystem::OpenEditor(std::string targetName, bool flatMode) {
        auto lock = ecs::World.StartTransaction<ecs::ReadAll,
            ecs::SendEventsLock,
            ecs::Write<ecs::Gui, ecs::TransformTree, ecs::Physics>>();

        auto inspector = inspectorEntity.Get(lock);

        if (!inspector.Has<ecs::TransformTree, ecs::Gui, ecs::Physics>(lock)) return;

        ecs::Entity target;
        if (targetName.empty()) {
            auto flatview = ecs::EntityWith<ecs::Name>(lock, ecs::Name("player", "flatview"));
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
            physics.group = ecs::PhysicsGroup::NoClip;
            return;
        }

        ecs::EventBindings::SendEvent(lock, "/edit/target", inspector, target);

        if (flatMode) {
            gui.target = ecs::GuiTarget::Debug;
            physics.group = ecs::PhysicsGroup::NoClip;
        } else {
            gui.target = ecs::GuiTarget::World;
            physics.group = ecs::PhysicsGroup::UserInterface;

            auto &transform = inspector.Get<ecs::TransformTree>(lock);

            auto player = playerEntity.Get(lock);
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
                transform.parent = playerEntity;
            }
        }
    }
} // namespace sp
