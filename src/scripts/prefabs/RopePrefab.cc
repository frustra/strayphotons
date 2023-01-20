#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <glm/glm.hpp>
#include <glm/gtx/vector_angle.hpp>

namespace sp::scripts {
    using namespace ecs;

    struct RopePrefab {
        uint32_t subDivisions = 10;
        float radius = 0.01f;
        float density = 1.0f;
        float linearDamping = 1.0f;
        float angularDamping = 10.0f;
        std::string model;
        size_t meshIndex = 0;
        Transform modelOffset;
        std::vector<glm::vec3> points, tangents;
        EntityRef startTarget, endTarget;

        void Prefab(const ScriptState &state,
            const std::shared_ptr<sp::Scene> &scene,
            Lock<AddRemove> lock,
            Entity ent) {
            Assertf(ent.Has<Name>(lock), "RopePrefab root has no name: %s", ToString(lock, ent));
            auto prefixName = ent.Get<Name>(lock);

            if (points.empty()) return;
            Assertf(points.size() == tangents.size(),
                "RopePrefab does not have equal length point/tangent lists: %s, (%u, %u)",
                ToString(lock, ent),
                points.size(),
                tangents.size());

            const float instanceLength = (points.size() - 1) / (float)subDivisions;
            float totalLength = 0.0f;
            float segmentStart = 0.0f;
            float nextInstance = instanceLength;
            Entity lastInstance = startTarget.Get(lock);
            glm::vec3 lastOffset = points[0];
            if (lastInstance) {
                lastOffset = glm::vec3(0);
                // lastInstance.Get<TransformTree>(lock).GetGlobalTransform(lock).GetPosition();
            }
            glm::vec3 lastPos = points[0];
            size_t i = 0;

            auto makeSegment = [&](glm::vec3 pos, glm::vec3 lastPos) {
                auto baseEnt =
                    scene->NewPrefabEntity(lock, ent, state.GetInstanceId(), "segment" + std::to_string(i), prefixName);

                float dist = glm::length(pos - lastPos);
                glm::vec3 dir = glm::normalize(pos - lastPos);
                auto rotation = glm::rotation(modelOffset.GetRight(), dir);

                auto scale = modelOffset.GetScale();
                scale.x *= dist;

                PhysicsShape shape(PhysicsShape::Capsule(std::max(0.0001f, dist - radius * 2.0f), radius),
                    Transform(glm::vec3(dist * 0.5f, 0, 0)));
                auto &ph = baseEnt.Set<Physics>(lock, shape, PhysicsGroup::World);
                ph.density = density;
                ph.linearDamping = linearDamping;
                ph.angularDamping = angularDamping;

                auto &joints = baseEnt.Set<PhysicsJoints>(lock);
                PhysicsJoint joint = {};
                joint.type = PhysicsJointType::Spherical;
                joint.target = lastInstance;
                joint.remoteOffset = lastOffset;
                joints.Add(joint);

                auto &transform = baseEnt.Set<TransformTree>(lock);
                transform.pose.SetPosition(pos - rotation * glm::vec3(dist, 0, 0));
                transform.pose.SetRotation(rotation);
                if (ent.Has<TransformTree>(lock)) transform.parent = ent;

                auto renderableEnt = scene->NewPrefabEntity(lock,
                    ent,
                    state.GetInstanceId(),
                    "segment" + std::to_string(i) + "_model",
                    prefixName);
                renderableEnt.Set<Renderable>(lock, model, meshIndex);
                auto &subTree = renderableEnt.Set<TransformTree>(lock, modelOffset);
                subTree.pose.matrix[3].x *= dist;
                subTree.pose.Scale(glm::vec3(dist, 1, 1));
                subTree.parent = baseEnt;

                lastInstance = baseEnt;
                lastOffset = glm::vec3(dist, 0, 0);
                totalLength += dist;
                i++;
            };

            for (size_t segment = 1; segment < points.size() && i + 1 < subDivisions; segment++) {
                auto &point = points[segment - 1];
                auto &nextPoint = points[segment];
                auto &tangent = tangents[segment - 1];
                auto &nextTangent = tangents[segment];

                float blendFactor = nextInstance - segmentStart;
                while (blendFactor <= 1.0 && i + 1 < subDivisions) {
                    glm::vec3 instancePos = CubicBlend(blendFactor, point, tangent, nextPoint, nextTangent);
                    if (instancePos != lastPos) {
                        makeSegment(instancePos, lastPos);
                    } else {
                        i++;
                    }

                    lastPos = instancePos;
                    nextInstance += instanceLength;
                    blendFactor = nextInstance - segmentStart;
                }

                segmentStart += 1.0f;
            }
            makeSegment(points[points.size() - 1], lastPos);
            PhysicsJoint joint = {};
            joint.type = PhysicsJointType::Spherical;
            joint.target = endTarget;
            joint.localOffset = lastOffset;
            joint.remoteOffset = endTarget ? glm::vec3(0) : points[points.size() - 1];
            lastInstance.Get<PhysicsJoints>(lock).Add(joint);
        }
    };
    StructMetadata MetadataRopePrefab(typeid(RopePrefab),
        StructField::New("segments", &RopePrefab::subDivisions),
        StructField::New("radius", &RopePrefab::radius),
        StructField::New("density", &RopePrefab::density),
        StructField::New("linear_damping", &RopePrefab::linearDamping),
        StructField::New("angular_damping", &RopePrefab::angularDamping),
        StructField::New("model", &RopePrefab::model),
        StructField::New("mesh_index", &RopePrefab::meshIndex),
        StructField::New("model_offset", &RopePrefab::modelOffset),
        StructField::New("start_target", &RopePrefab::startTarget),
        StructField::New("end_target", &RopePrefab::endTarget),
        StructField::New("points", &RopePrefab::points),
        StructField::New("tangents", &RopePrefab::tangents));
    PrefabScript<RopePrefab> ropePrefab("rope", MetadataRopePrefab);
} // namespace sp::scripts
