#include "EditorSystem.hh"

#include "console/Console.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/SceneManager.hh"

namespace sp {
    EditorSystem::EditorSystem() : RegisteredThread("EditorSystem", 30.0) {
        funcs.Register(this,
            "edit",
            "Edit the specified entity, or the entity being looked at",
            &EditorSystem::OpenEditor);
        StartThread();
    }

    EditorSystem::~EditorSystem() {
        StopThread();

        GetSceneManager().QueueActionAndBlock(SceneAction::RemoveScene, "editor");
    }

    void EditorSystem::OpenEditor(std::string targetName) {
        auto lock = ecs::World.StartTransaction<ecs::ReadAll>();
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
        if (!entity) Errorf("Entity not found: %s", targetName);
    }

    bool EditorSystem::ThreadInit() {
        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "editor",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto inspector = scene->NewSystemEntity(lock, scene, inspectorEntity.Name());
                auto &gui = inspector.Set<ecs::Gui>(lock, "inspector");
                gui.disabled = true;
                inspector.Set<ecs::Screen>(lock);
                inspector.Set<ecs::EventInput>(lock, "/interact/point", "/interact/press");
                inspector.Set<ecs::Physics>(lock,
                    ecs::PhysicsShape::Box(glm::vec3(1, 1, 0.01)),
                    ecs::PhysicsGroup::UserInterface,
                    false /* dynamic */);

                auto &transform = inspector.Set<ecs::TransformTree>(lock);
                transform.parent = ecs::Name("player", "player");
                transform.pose.Translate(glm::vec3(0, 1, -1));
            });

        return true;
    }

    void EditorSystem::PreFrame() {}
    void EditorSystem::Frame() {}
} // namespace sp
