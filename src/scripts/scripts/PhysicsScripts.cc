#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace sp::scripts {
    using namespace ecs;

    struct VoxelController {
        float voxelScale = 0.1f;
        float voxelStride = 1.0f;
        glm::vec3 voxelOffset;
        EntityRef alignmentEntity, followEntity;
        std::optional<glm::vec3> alignment;

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TransformTree, VoxelArea>(lock) || voxelScale == 0.0f || voxelStride < 1.0f) return;

            auto &transform = ent.Get<TransformTree>(lock);
            auto &voxelArea = ent.Get<VoxelArea>(lock);
            auto voxelRotation = transform.GetGlobalRotation(lock);

            glm::vec3 offset = voxelOffset * glm::vec3(voxelArea.extents) * voxelScale;
            auto alignmentTarget = alignmentEntity.Get(lock);
            if (alignmentTarget.Has<TransformSnapshot>(lock)) {
                glm::vec3 alignmentOffset;
                if (alignment) {
                    alignmentOffset = alignmentTarget.Get<TransformSnapshot>(lock).GetPosition() - alignment.value();
                } else {
                    alignment = alignmentTarget.Get<TransformSnapshot>(lock).GetPosition();
                }
                offset += glm::mod(alignmentOffset, voxelStride * voxelScale);
            }

            auto followPosition = glm::vec3(0);
            auto followTarget = followEntity.Get(lock);
            if (followTarget.Has<TransformSnapshot>(lock)) {
                followPosition = followTarget.Get<TransformSnapshot>(lock).GetPosition();
            }

            followPosition = voxelRotation * followPosition;
            followPosition = glm::floor(followPosition / voxelStride / voxelScale) * voxelScale * voxelStride;
            transform.pose.SetPosition(glm::inverse(voxelRotation) * (followPosition + offset));
            transform.pose.SetScale(glm::vec3(voxelScale));
        }
    };
    StructMetadata MetadataVoxelController(typeid(VoxelController),
        StructField::New("voxel_scale", &VoxelController::voxelScale),
        StructField::New("voxel_stride", &VoxelController::voxelStride),
        StructField::New("voxel_offset", &VoxelController::voxelOffset),
        StructField::New("alignment_target", &VoxelController::alignmentEntity),
        StructField::New("follow_target", &VoxelController::followEntity));
    InternalPhysicsScript<VoxelController> voxelController("voxel_controller", MetadataVoxelController);

    struct RotatePhysics {
        glm::vec3 rotationAxis;
        float rotationSpeedRpm;

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TransformTree>(lock) || rotationAxis == glm::vec3(0) || rotationSpeedRpm == 0.0f) return;

            auto &transform = ent.Get<TransformTree>(lock);
            auto currentRotation = transform.pose.GetRotation();
            transform.pose.SetRotation(glm::rotate(currentRotation,
                (float)(rotationSpeedRpm * M_PI * 2.0 / 60.0 * interval.count() / 1e9),
                rotationAxis));
        }
    };
    StructMetadata MetadataRotatePhysics(typeid(RotatePhysics),
        StructField::New("axis", &RotatePhysics::rotationAxis),
        StructField::New("speed", &RotatePhysics::rotationSpeedRpm));
    InternalPhysicsScript<RotatePhysics> rotatePhysics("rotate_physics", MetadataRotatePhysics);

    struct PhysicsJointFromEvent {
        robin_hood::unordered_map<std::string, ecs::PhysicsJoint> definedJoints;

        void Init(ScriptState &state) {
            state.definition.events.clear();
            state.definition.events.reserve(definedJoints.size() * 5);
            for (auto &[name, _] : definedJoints) {
                state.definition.events.emplace_back("/physics_joint/" + name + "/enable"); // bool
                state.definition.events.emplace_back("/physics_joint/" + name + "/set_target"); // EntityRef
                state.definition.events.emplace_back("/physics_joint/" + name + "/set_current_offset"); // EntityRef
                state.definition.events.emplace_back("/physics_joint/" + name + "/set_local_offset"); // Transform
                state.definition.events.emplace_back("/physics_joint/" + name + "/set_remote_offset"); // Transform
            }
            state.definition.filterOnEvent = true; // Effective next tick, only run when events arrive.
        }

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<ecs::Physics, ecs::PhysicsJoints>(lock)) return;

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                Assertf(sp::starts_with(event.name, "/physics_joint/"),
                    "Event name should be /physics_joint/<name>/<action>");
                auto eventName = std::string_view(event.name).substr("/physics_joint/"s.size());
                auto delimiter = eventName.find('/');
                Assertf(delimiter != std::string_view::npos, "Event name should be /physics_joint/<name>/<action>");
                std::string jointName(eventName.substr(0, delimiter));
                auto action = eventName.substr(delimiter + 1);
                if (jointName.empty()) continue;

                auto it = definedJoints.find(jointName);
                if (it == definedJoints.end()) continue;
                auto &joint = it->second;
                auto &joints = ent.Get<ecs::PhysicsJoints>(lock).joints;
                auto existing = std::find_if(joints.begin(), joints.end(), [&joint](auto &arg) {
                    return arg.target == joint.target && arg.type == joint.type;
                });

                if (action == "enable") {
                    bool enabled = std::visit(
                        [](auto &arg) {
                            using T = std::decay_t<decltype(arg)>;

                            if constexpr (std::is_convertible_v<double, T> && std::is_convertible_v<T, double>) {
                                return (double)arg >= 0.5;
                            } else if constexpr (std::is_convertible_v<T, bool>) {
                                return (bool)arg;
                            } else {
                                return true;
                            }
                        },
                        event.data);
                    if (existing == joints.end()) {
                        if (enabled) existing = joints.insert(joints.end(), joint);
                    } else if (!enabled) {
                        joints.erase(existing);
                        existing = joints.end();
                    }
                } else if (action == "set_target") {
                    if (std::holds_alternative<std::string>(event.data)) {
                        auto targetName = std::get<std::string>(event.data);
                        joint.target = ecs::Name(targetName, ecs::Name());
                    } else if (std::holds_alternative<EntityRef>(event.data)) {
                        joint.target = std::get<EntityRef>(event.data);
                    } else {
                        Errorf("Invalid set_target event type: %s", event.toString());
                        continue;
                    }
                } else if (action == "set_current_offset") {
                    joint.localOffset = Transform();
                    Entity target;
                    if (std::holds_alternative<std::string>(event.data)) {
                        auto targetName = std::get<std::string>(event.data);
                        target = ecs::EntityRef(ecs::Name(targetName, state.scope)).Get(lock);
                    } else if (std::holds_alternative<EntityRef>(event.data)) {
                        target = std::get<EntityRef>(event.data).Get(lock);
                    } else {
                        Errorf("Invalid set_current_offset event type: %s", event.toString());
                        continue;
                    }
                    if (ent.Has<TransformSnapshot>(lock) && target.Has<TransformSnapshot>(lock)) {
                        joint.localOffset = ent.Get<ecs::TransformSnapshot>(lock).GetInverse() *
                                            target.Get<ecs::TransformSnapshot>(lock);
                    }
                } else if (action == "set_local_offset") {
                    if (std::holds_alternative<Transform>(event.data)) {
                        joint.localOffset = std::get<Transform>(event.data);
                    } else {
                        Errorf("Invalid set_local_offset event type: %s", event.toString());
                        continue;
                    }
                } else if (action == "set_remote_offset") {
                    if (std::holds_alternative<Transform>(event.data)) {
                        joint.remoteOffset = std::get<Transform>(event.data);
                    } else {
                        Errorf("Invalid set_remote_offset event type: %s", event.toString());
                        continue;
                    }
                } else {
                    Errorf("Unknown signal action: '%s'", std::string(action));
                }
                if (existing != joints.end()) {
                    *existing = joint;
                }
            }
        }
    };
    StructMetadata MetadataPhysicsJointFromEvent(typeid(PhysicsJointFromEvent),
        StructField::New(&PhysicsJointFromEvent::definedJoints));
    InternalPhysicsScript<PhysicsJointFromEvent> physicsJointFromEvent("physics_joint_from_event",
        MetadataPhysicsJointFromEvent);
} // namespace sp::scripts
