#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <cmath>
#include <glm/glm.hpp>

namespace sp::scripts {
    using namespace ecs;

    struct EdgeTrigger {
        // Input parameters
        std::string inputExpr;
        std::string outputName = "/script/edge_trigger";

        // Internal script state
        SignalExpression expr;
        double previousValue;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (expr.expr != inputExpr) {
                expr = SignalExpression(inputExpr, state.scope);
                previousValue = expr.Evaluate(lock);
            }

            auto value = expr.Evaluate(lock);
            if (value >= 0.5 && previousValue < 0.5) {
                EventBindings::SendEvent(lock, ent, Event{outputName, ent, true});
            } else if (value < 0.5 && previousValue >= 0.5) {
                EventBindings::SendEvent(lock, ent, Event{outputName, ent, false});
            }
            previousValue = value;
        }
    };
    StructMetadata MetadataEdgeTrigger(typeid(EdgeTrigger),
        StructField::New("input_expr", &EdgeTrigger::inputExpr),
        StructField::New("output_event", &EdgeTrigger::outputName));
    InternalScript<EdgeTrigger> edgeTrigger("edge_trigger", MetadataEdgeTrigger);

    struct ModelSpawner {
        EntityRef targetEntity;
        glm::vec3 position;
        std::string modelName;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            Transform relativeTransform;
            auto target = targetEntity.Get(lock);
            if (target.Has<TransformSnapshot>(lock)) {
                relativeTransform = target.Get<TransformSnapshot>(lock);
            }

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (event.name != "/script/spawn") continue;

                Transform transform(position);
                transform = relativeTransform * transform;

                GetSceneManager().QueueAction(SceneAction::EditLiveECS,
                    [ent, transform, modelName = modelName, scope = state.scope](ecs::Lock<ecs::AddRemove> lock) {
                        if (!ent.Has<ecs::SceneInfo>(lock)) return;
                        auto &sceneInfo = ent.Get<ecs::SceneInfo>(lock);
                        auto scene = sceneInfo.scene.ptr.lock();
                        if (!scene) return;

                        auto newEntity = scene->NewRootEntity(lock, scene);

                        newEntity.Set<TransformTree>(lock, transform);
                        newEntity.Set<TransformSnapshot>(lock, transform);
                        newEntity.Set<Renderable>(lock, modelName, sp::Assets().LoadGltf(modelName));
                        newEntity.Set<Physics>(lock, modelName, PhysicsGroup::World, true, 1.0f);
                        newEntity.Set<PhysicsJoints>(lock);
                        newEntity.Set<PhysicsQuery>(lock);
                        newEntity.Set<EventInput>(lock);
                        auto &scripts = newEntity.Set<Scripts>(lock);
                        scripts.AddOnTick(scope, "interactive_object");
                    });
            }
        }
    };
    StructMetadata MetadataModelSpawner(typeid(ModelSpawner),
        StructField::New("relative_to", &ModelSpawner::targetEntity),
        StructField::New("position", &ModelSpawner::position),
        StructField::New("model", &ModelSpawner::modelName));
    InternalScript<ModelSpawner> modelSpawner("model_spawner", MetadataModelSpawner, true, "/script/spawn");

    struct Rotate {
        glm::vec3 rotationAxis;
        float rotationSpeedRpm;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TransformTree>(lock) || rotationAxis == glm::vec3(0) || rotationSpeedRpm == 0.0f) return;

            auto &transform = ent.Get<TransformTree>(lock);
            auto currentRotation = transform.pose.GetRotation();
            transform.pose.SetRotation(glm::rotate(currentRotation,
                (float)(rotationSpeedRpm * M_PI * 2.0 / 60.0 * interval.count() / 1e9),
                rotationAxis));
        }
    };
    StructMetadata MetadataRotate(typeid(Rotate),
        StructField::New("axis", &Rotate::rotationAxis),
        StructField::New("speed", &Rotate::rotationSpeedRpm));
    InternalScript<Rotate> rotate("rotate", MetadataRotate);

    struct EmissiveFromSignal {
        SignalExpression signalExpr;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<Renderable>(lock)) return;

            auto signalValue = signalExpr.Evaluate(lock);
            ent.Get<Renderable>(lock).emissiveScale = signalValue;
        }
    };
    StructMetadata MetadataEmissiveFromSignal(typeid(EmissiveFromSignal),
        StructField::New("signal_expr", &EmissiveFromSignal::signalExpr));
    InternalScript<EmissiveFromSignal> emissiveFromSignal("emissive_from_signal", MetadataEmissiveFromSignal);
} // namespace sp::scripts
