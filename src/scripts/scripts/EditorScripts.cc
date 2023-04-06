#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"

namespace sp::scripts {
    using namespace ecs;

    struct TraySpawner {
        std::string templateSource;

        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            Event event;
            while (EventInput::Poll(entLock, state.eventQueue, event)) {
                if (event.name != INTERACT_EVENT_INTERACT_GRAB) continue;

                // Ignore drop events
                if (!std::holds_alternative<Transform>(event.data)) continue;

                if (templateSource.empty()) {
                    Errorf("TraySpawner missing source parameter");
                    continue;
                }

                if (!entLock.Has<Name, TransformTree>()) continue;
                auto &sourceName = entLock.Get<Name>();
                auto transform = entLock.Get<TransformTree>().GetGlobalTransform(entLock);
                std::optional<SignalOutput> signals;
                if (entLock.Has<SignalOutput>()) signals = entLock.Get<SignalOutput>();

                SceneRef scene;
                if (entLock.Has<ActiveScene>()) {
                    auto &active = entLock.Get<ActiveScene>();
                    scene = active.scene;
                }
                if (!scene) {
                    Errorf("TraySpawner has no active scene");
                    continue;
                }

                auto sharedEntity = make_shared<EntityRef>();
                GetSceneManager().QueueAction(SceneAction::EditStagingScene,
                    scene.data->name,
                    [source = templateSource, transform, signals, baseName = sourceName.entity, sharedEntity](
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
                        if (signals) {
                            newEntity.Set<SignalOutput>(lock, *signals);
                        }
                        auto &scripts = newEntity.Set<Scripts>(lock);
                        auto &prefab = scripts.AddPrefab(Name(scene->data->name, ""), "template");
                        prefab.SetParam("source", source);
                        ecs::Scripts::RunPrefabs(lock, newEntity);

                        *sharedEntity = newEntity;
                    });
                GetSceneManager().QueueAction(SceneAction::ApplyStagingScene, scene.data->name);
                GetSceneManager().QueueAction([ent = entLock.entity, target = event.source, sharedEntity] {
                    auto lock = ecs::StartTransaction<ecs::SendEventsLock>();
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
