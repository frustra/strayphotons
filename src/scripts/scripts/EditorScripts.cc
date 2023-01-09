#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/SceneManager.hh"

namespace sp::scripts {
    using namespace ecs;

    struct TraySpawner {
        std::string templateSource;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (event.name != INTERACT_EVENT_INTERACT_GRAB) continue;

                // Ignore drop events
                if (!std::holds_alternative<Transform>(event.data)) continue;

                if (templateSource.empty()) {
                    Errorf("TraySpawner missing source parameter");
                    continue;
                }

                if (!ent.Has<Name, TransformTree>(lock)) continue;
                auto &sourceName = ent.Get<Name>(lock);
                auto transform = ent.Get<TransformTree>(lock).GetGlobalTransform(lock);

                SceneRef scene;
                if (lock.Has<ActiveScene>()) {
                    auto &active = lock.Get<ActiveScene>();
                    scene = active.scene;
                }
                if (!scene) {
                    Errorf("TraySpawner has no active scene");
                    continue;
                }

                auto sharedEntity = make_shared<EntityRef>();
                GetSceneManager().QueueAction(SceneAction::EditStagingScene,
                    scene.data->name,
                    [source = templateSource, transform, baseName = sourceName.entity, sharedEntity](
                        ecs::Lock<ecs::AddRemove> lock,
                        std::shared_ptr<Scene> scene) {
                        Name name(scene->data->name, "");
                        for (size_t i = 0;; i++) {
                            name.entity = baseName + "_" + std::to_string(i);
                            if (!scene->GetStagingEntity(name)) break;
                        }
                        Logf("TraySpawner new entity: %s", name.String());

                        auto newEntity = scene->NewRootEntity(lock, scene, name.entity);
                        newEntity.Set<TransformTree>(lock, transform);
                        auto &scripts = newEntity.Set<Scripts>(lock);
                        auto &prefab = scripts.AddPrefab(Name(scene->data->name, ""), "template");
                        prefab.SetParam("source", source);
                        ecs::Scripts::RunPrefabs(lock, newEntity);

                        *sharedEntity = newEntity;
                    });
                GetSceneManager().QueueAction(SceneAction::ApplyStagingScene,
                    scene.data->name,
                    [ent, target = event.source, sharedEntity](auto lock) {
                        ecs::EventBindings::SendEvent(lock,
                            target,
                            ecs::Event{INTERACT_EVENT_INTERACT_GRAB, ent, sharedEntity->Get(lock)});
                    });
            }
        }
    };
    StructMetadata MetadataTraySpawner(typeid(TraySpawner), StructField::New("source", &TraySpawner::templateSource));
    InternalScript<TraySpawner> traySpawner("tray_spawner", MetadataTraySpawner, true, INTERACT_EVENT_INTERACT_GRAB);
} // namespace sp::scripts
