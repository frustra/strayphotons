#include "ecs/systems/DoorSystem.hh"

#include <core/Logging.hh>
#include <ecs/Ecs.hh>
#include <ecs/EcsImpl.hh>

namespace ecs {
    DoorSystem::DoorSystem(ecs::ECS &ecs) : ecs(ecs) {}

    bool DoorSystem::Frame(float dtSinceLastFrame) {
        auto lock = ecs.StartTransaction<Read<SlideDoor, SignalReceiver>, Write<Animation>>();
        for (Tecs::Entity ent : lock.EntitiesWith<SlideDoor>()) {
            auto &receiver = ent.Get<SignalReceiver>(lock);
            auto &door = ent.Get<SlideDoor>(lock);

            SlideDoor::State state = door.GetState(lock);
            if (receiver.IsTriggered()) {
                if (state != SlideDoor::State::OPENED && state != SlideDoor::State::OPENING) { door.Open(lock); }
            } else {
                if (state != SlideDoor::State::CLOSED && state != SlideDoor::State::CLOSING) { door.Close(lock); }
            }
        }
        return true;
    }
} // namespace ecs
