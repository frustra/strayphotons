#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"

#include <glm/gtx/component_wise.hpp>

namespace sp::scripts {
    using namespace ecs;

    static CVar<float> CVarMaxMagentForce("i.MaxMagnetForce", 1.0f, "Maximum force applied to magnetic objects");
    static CVar<float> CVarMaxMagentTorque("i.MaxMagnetTorque", 0.01f, "Maximum torque applied to magnetic objects");

    std::array magnetScripts = {
        InternalScript("magnetic_plug",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (!ent.Has<TransformTree, PhysicsJoints>(lock)) return;
                auto transform = ent.Get<TransformTree>(lock).GetGlobalTransform(lock);
                auto &joints = ent.Get<PhysicsJoints>(lock);

                Event event;
                while (EventInput::Poll(lock, ent, "/magnetic/nearby", event)) {
                    auto magnetRadius = std::get_if<float>(&event.data);
                    if (magnetRadius) {
                        PhysicsJoint joint;
                        joint.target = event.source;
                        joint.type = PhysicsJointType::Force;
                        joint.limit = glm::vec2(CVarMaxMagentForce.Get(), CVarMaxMagentTorque.Get());
                        joint.magnetRadius = *magnetRadius;
                        joints.Add(joint);
                    } else {
                        sp::erase_if(joints.joints, [&](auto &&joint) {
                            return joint.target == event.source;
                        });
                    }
                }
            }),
        InternalScript("magnetic_socket",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (!ent.Has<TransformSnapshot>(lock)) return;
                auto &transform = ent.Get<TransformSnapshot>(lock);
                float radius = glm::compMin(transform.GetScale());

                Event event;
                while (EventInput::Poll(lock, ent, "/trigger/magnetic/enter", event)) {
                    auto data = std::get_if<Entity>(&event.data);
                    if (data) {
                        EventBindings::SendEvent(lock,
                            *data,
                            "/magnetic/nearby",
                            Event{"/magnetic/nearby", ent, radius});
                    }
                }
                while (EventInput::Poll(lock, ent, "/trigger/magnetic/leave", event)) {
                    auto data = std::get_if<Entity>(&event.data);
                    if (data) {
                        EventBindings::SendEvent(lock,
                            *data,
                            "/magnetic/nearby",
                            Event{"/magnetic/nearby", ent, false});
                    }
                }
            }),
    };
} // namespace sp::scripts
