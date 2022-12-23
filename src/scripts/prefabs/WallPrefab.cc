#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <glm/glm.hpp>
#include <glm/gtx/vector_angle.hpp>

namespace sp::scripts {
    using namespace ecs;

    struct Wall {
        float yOffset = 0.0f;
        float stride = 1.0f;
        std::vector<glm::vec2> segmentPoints;
        std::vector<std::string> segmentTypes;

        void Prefab(const ScriptState &state, Lock<AddRemove> lock, Entity ent) {
            auto scene = state.scope.scene.lock();
            Assertf(scene, "wall prefab does not have a valid scene: %s", ToString(lock, ent));
            Assertf(ent.Has<Name>(lock), "Wall prefab root has no name: %s", ToString(lock, ent));
            auto prefixName = ent.Get<Name>(lock);

            Assertf(segmentPoints.size() == segmentTypes.size() + 1,
                "wall prefab does not have equal length segment lists: %s, (%u, %u)",
                ToString(lock, ent),
                segmentPoints.size(),
                segmentTypes.size());

            glm::vec2 lastDir = glm::vec2(1, 0);
            for (size_t segment = 1; segment < segmentPoints.size(); segment++) {
                glm::vec2 point = segmentPoints[segment - 1];
                glm::vec2 dir = segmentPoints[segment] - point;
                auto distance = glm::length(dir);
                dir = glm::normalize(dir);
                auto rotation = glm::orientedAngle(dir, glm::vec2(1, 0));

                if (segment > 1 && dir != lastDir) {
                    auto newEnt = scene->NewPrefabEntity(lock,
                        ent,
                        state.GetInstanceId(),
                        "corner" + std::to_string(segment),
                        prefixName);

                    auto deltaRotation = glm::orientedAngle(lastDir, dir);
                    auto &transform = newEnt.Set<TransformTree>(lock,
                        glm::vec3(point.x, yOffset, point.y),
                        glm::quat(glm::vec3(0, rotation + deltaRotation / 2, 0)));
                    if (ent.Has<TransformTree>(lock)) transform.parent = ent;

                    auto &scripts = newEnt.Set<Script>(lock);
                    auto &gltfState = scripts.AddPrefab(state.scope, "gltf");
                    gltfState.SetParam<std::string>("model", "wall-4-corner");
                    gltfState.SetParam<std::string>("physics", "static");
                    gltfState.SetParam<bool>("render", true);
                    scripts.Prefab(lock, newEnt);
                }
                lastDir = dir;

                std::string model = segmentTypes[segment - 1];

                point += dir * stride * 0.5f;
                size_t count = (size_t)std::floor(distance / stride);
                for (size_t i = 0; i < count; i++) {
                    auto newEnt = scene->NewPrefabEntity(lock,
                        ent,
                        state.GetInstanceId(),
                        "segment" + std::to_string(segment) + "_" + std::to_string(i),
                        prefixName);

                    auto &transform = newEnt.Set<TransformTree>(lock,
                        glm::vec3(point.x, yOffset, point.y),
                        glm::quat(glm::vec3(0, rotation, 0)));
                    if (ent.Has<TransformTree>(lock)) transform.parent = ent;

                    auto &scripts = newEnt.Set<Script>(lock);
                    auto &gltfState = scripts.AddPrefab(state.scope, "gltf");
                    gltfState.SetParam<std::string>("model", model);
                    gltfState.SetParam<std::string>("physics", "static");
                    gltfState.SetParam<bool>("render", true);
                    scripts.Prefab(lock, newEnt);

                    point += dir * stride;
                }
            }
        }
    };
    StructMetadata MetadataWall(typeid(Wall),
        StructField::New("y_offset", &Wall::yOffset),
        StructField::New("stride", &Wall::stride),
        StructField::New("segments", &Wall::segmentPoints),
        StructField::New("segment_types", &Wall::segmentTypes));
    PrefabScript<Wall> wall("wall", MetadataWall);
} // namespace sp::scripts
