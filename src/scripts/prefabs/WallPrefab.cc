/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <glm/glm.hpp>
#include <glm/gtx/vector_angle.hpp>

namespace sp::scripts {
    using namespace ecs;

    struct WallPrefab {
        float yOffset = 0.0f;
        float stride = 1.0f;
        std::vector<glm::vec2> segmentPoints;
        std::vector<std::string> segmentTypes;

        void Prefab(const ScriptState &state,
            const std::shared_ptr<sp::Scene> &scene,
            Lock<AddRemove> lock,
            Entity ent) {
            ZoneScoped;
            Assertf(ent.Has<Name>(lock), "WallPrefab root has no name: %s", ToString(lock, ent));
            auto prefixName = ent.Get<Name>(lock);

            Assertf(segmentPoints.size() == segmentTypes.size() + 1,
                "WallPrefab does not have equal length segment lists: %s, (%u, %u)",
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

                    auto &scripts = newEnt.Set<Scripts>(lock);
                    auto &gltfState = scripts.AddScript(state.scope, "prefab_gltf");
                    gltfState.SetParam<std::string>("model", "wall-4-corner");
                    gltfState.SetParam<std::optional<PhysicsActorType>>("physics", PhysicsActorType::Static);
                    gltfState.SetParam<bool>("render", true);
                    ecs::GetScriptManager().RunPrefabs(lock, newEnt);
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

                    auto &scripts = newEnt.Set<Scripts>(lock);
                    auto &gltfState = scripts.AddScript(state.scope, "prefab_gltf");
                    gltfState.SetParam<std::string>("model", model);
                    gltfState.SetParam<std::optional<PhysicsActorType>>("physics", PhysicsActorType::Static);
                    gltfState.SetParam<bool>("render", true);
                    ecs::GetScriptManager().RunPrefabs(lock, newEnt);

                    point += dir * stride;
                }
            }
        }
    };
    StructMetadata MetadataWallPrefab(typeid(WallPrefab),
        sizeof(WallPrefab),
        "WallPrefab",
        "",
        StructField::New("y_offset", &WallPrefab::yOffset),
        StructField::New("stride", &WallPrefab::stride),
        StructField::New("segments", &WallPrefab::segmentPoints),
        StructField::New("segment_types", &WallPrefab::segmentTypes));
    PrefabScript<WallPrefab> wallPrefab("prefab_wall", MetadataWallPrefab);
} // namespace sp::scripts
