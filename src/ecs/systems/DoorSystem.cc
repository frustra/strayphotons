#include "ecs/systems/DoorSystem.hh"

#include "core/Logging.hh"

#include <ecs/Ecs.hh>
#include <ecs/EcsImpl.hh>

namespace ecs {
	DoorSystem::DoorSystem(ecs::EntityManager &em) : entities(em) {}

	bool DoorSystem::Frame(float dtSinceLastFrame) {
		for (Entity ent : entities.EntitiesWith<SlideDoor, SignalReceiver>()) {
			auto receiver = ent.Get<SignalReceiver>();
			auto door = ent.Get<SlideDoor>();

			SlideDoor::State state = door->GetState(entities);
			if (receiver->IsTriggered()) {
				if (state != SlideDoor::State::OPENED && state != SlideDoor::State::OPENING) { door->Open(entities); }
			} else {
				if (state != SlideDoor::State::CLOSED && state != SlideDoor::State::CLOSING) { door->Close(entities); }
			}
		}
		return true;
	}
} // namespace ecs
