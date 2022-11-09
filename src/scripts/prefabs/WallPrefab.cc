#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <glm/glm.hpp>
#include <glm/gtx/vector_angle.hpp>

namespace ecs {
    InternalPrefab wallPrefab("wall", [](const ScriptState &state, Lock<AddRemove> lock, Entity ent) {
        auto scene = state.scope.scene.lock();
        Assertf(scene, "wall prefab does not have a valid scene: %s", ToString(lock, ent));
        if (!state.HasParam<vector<double>>("segments_x") || !state.HasParam<vector<double>>("segments_y")) return;
        if (!state.HasParam<vector<string>>("segment_types") || !state.HasParam<double>("stride")) return;
        Assertf(ent.Has<Name>(lock), "Wall prefab root has no name: %s", ToString(lock, ent));
        auto &prefixName = ent.Get<Name>(lock);

        float yOffset = state.GetParam<double>("y_offset");
        float stride = state.GetParam<double>("stride");

        auto &segmentsX = state.GetParamRef<vector<double>>("segments_x");
        auto &segmentsY = state.GetParamRef<vector<double>>("segments_y");
        auto &segmentTypes = state.GetParamRef<vector<string>>("segment_types");

        Assertf(segmentsX.size() == segmentsY.size() && segmentsX.size() == segmentTypes.size() + 1,
            "wall prefab does not have equal length segment lists: %s, (%u, %u, %u)",
            ToString(lock, ent),
            segmentsX.size(),
            segmentsY.size(),
            segmentTypes.size());

        glm::vec2 lastDir = glm::vec2(1, 0);
        for (size_t segment = 1; segment < segmentsX.size(); segment++) {
            glm::vec2 point(segmentsX[segment - 1], segmentsY[segment - 1]);
            glm::vec2 nextPoint(segmentsX[segment], segmentsY[segment]);
            glm::vec2 dir = nextPoint - point;
            auto distance = glm::length(dir);
            dir = glm::normalize(dir);
            auto rotation = glm::orientedAngle(dir, glm::vec2(1, 0));

            if (segment > 1 && dir != lastDir) {
                auto newEnt = scene->NewPrefabEntity(lock, ent, "corner" + std::to_string(segment), prefixName);

                auto deltaRotation = glm::orientedAngle(lastDir, dir);
                TransformTree transform(glm::vec3(point.x, yOffset, point.y),
                    glm::quat(glm::vec3(0, rotation + deltaRotation / 2, 0)));
                if (ent.Has<TransformTree>(lock)) transform.parent = ent;
                LookupComponent<TransformTree>().ApplyComponent(transform, lock, newEnt);

                Script scriptComp;
                auto &gltfState = scriptComp.AddPrefab(state.scope, "gltf");
                gltfState.SetParam<std::string>("model", "wall-4-corner");
                gltfState.SetParam<std::string>("physics", "static");
                gltfState.SetParam<bool>("render", true);
                LookupComponent<Script>().ApplyComponent(scriptComp, lock, newEnt);
                newEnt.Get<Script>(lock).Prefab(lock, newEnt);
            }
            lastDir = dir;

            std::string model = segmentTypes[segment - 1];

            point += dir * stride * 0.5f;
            size_t count = (size_t)std::floor(distance / stride);
            for (size_t i = 0; i < count; i++) {
                auto newEnt = scene->NewPrefabEntity(lock,
                    ent,
                    "segment" + std::to_string(segment) + "_" + std::to_string(i),
                    prefixName);

                TransformTree transform(glm::vec3(point.x, yOffset, point.y), glm::quat(glm::vec3(0, rotation, 0)));
                if (ent.Has<TransformTree>(lock)) transform.parent = ent;

                LookupComponent<TransformTree>().ApplyComponent(transform, lock, newEnt);

                Script scriptComp;
                auto &gltfState = scriptComp.AddPrefab(state.scope, "gltf");
                gltfState.SetParam<std::string>("model", model);
                gltfState.SetParam<std::string>("physics", "static");
                gltfState.SetParam<bool>("render", true);
                LookupComponent<Script>().ApplyComponent(scriptComp, lock, newEnt);
                newEnt.Get<Script>(lock).Prefab(lock, newEnt);

                point += dir * stride;
            }
        }
    });
} // namespace ecs
