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

    EditorSystem GEditor;

    EditorSystem::EditorSystem() : RegisteredThread("EditorSystem", 30.0), workQueue("EditorSystem", 1) {
        funcs.Register(this,
            "edit",
            "Edit the specified entity, or the entity being looked at",
            &EditorSystem::OpenEditor);

        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "editor",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto inspector = scene->NewSystemEntity(lock, scene, inspectorEntity.Name());
                auto &gui = inspector.Set<ecs::Gui>(lock, "inspector");
                gui.disabled = true;
                inspector.Set<ecs::Screen>(lock);
                inspector.Set<ecs::EventInput>(lock,
                    INTERACT_EVENT_INTERACT_POINT,
                    INTERACT_EVENT_INTERACT_PRESS,
                    EDITOR_EVENT_EDIT_TARGET);
                inspector.Set<ecs::Physics>(lock,
                    ecs::PhysicsShape::Box(glm::vec3(1, 1, 0.01)),
                    ecs::PhysicsGroup::UserInterface,
                    false /* dynamic */);
                inspector.Set<ecs::TransformTree>(lock);
            });

        StartThread();
    }

    EditorSystem::~EditorSystem() {
        StopThread();
        GetSceneManager().QueueActionAndBlock(SceneAction::RemoveScene, "editor");
    }

    void EditorSystem::OpenEditor(std::string targetName) {
        auto lock = ecs::World.StartTransaction<ecs::ReadAll, ecs::SendEventsLock, ecs::Write<ecs::Gui>>();
        auto inspector = inspectorEntity.Get(lock);

        ecs::Entity entity;
        if (targetName.empty()) {
            auto flatview = ecs::EntityWith<ecs::Name>(lock, ecs::Name("player", "flatview"));
            if (flatview.Has<ecs::PhysicsQuery>(lock)) {
                auto &query = flatview.Get<ecs::PhysicsQuery>(lock);
                for (auto &subQuery : query.queries) {
                    auto *raycastQuery = std::get_if<ecs::PhysicsQuery::Raycast>(&subQuery);
                    if (raycastQuery && raycastQuery->result) {
                        entity = raycastQuery->result->target;
                        if (entity) break;
                    }
                }
            }
        } else {
            ecs::EntityRef ref = ecs::Name(targetName, ecs::Name());
            entity = ref.Get(lock);
        }
        targetEntity = entity;
        ecs::EventBindings::SendEvent(lock, "/edit/target", inspector, entity);
    }

    void EditorSystem::Frame() {
        auto lock =
            ecs::World.StartTransaction<ecs::Read<ecs::TransformSnapshot>, ecs::Write<ecs::TransformTree, ecs::Gui>>();

        auto inspector = inspectorEntity.Get(lock);
        auto player = playerEntity.Get(lock);
        if (!player.Has<ecs::TransformSnapshot>(lock) || !inspector.Has<ecs::TransformTree, ecs::Gui>(lock)) {
            return;
        }

        auto &transform = inspector.Get<ecs::TransformTree>(lock);
        auto &gui = inspector.Get<ecs::Gui>(lock);

        auto target = targetEntity.Get(lock);
        if (!target.Exists(lock)) {
            gui.disabled = true;
            previousTargetEntity = {};
            return;
        } else if (target == previousTargetEntity) {
            return;
        }

        previousTargetEntity = target;
        gui.disabled = false;
        if (target.Has<ecs::TransformSnapshot>(lock)) {
            auto targetPos = target.Get<ecs::TransformSnapshot>(lock).GetPosition();
            auto playerPos = player.Get<ecs::TransformSnapshot>(lock).GetPosition();
            auto targetDir = glm::normalize(glm::vec3(targetPos.x - playerPos.x, 0, targetPos.z - playerPos.z));
            transform.pose.SetPosition(
                playerPos + targetDir * CVarEditorDistance.Get() + glm::vec3(0, CVarEditorOffset.Get(), 0));
            transform.pose.SetRotation(
                glm::quat(glm::vec3(glm::radians(CVarEditorAngle.Get()), glm::atan(-targetDir.x, -targetDir.z), 0)));
            transform.parent = {};
        } else {
            transform.pose = ecs::Transform(glm::vec3(0, 1, -1));
            transform.parent = playerEntity;
        }
    }
} // namespace sp
