#include "ecs/systems/DoorSystem.hh"

#include "core/Logging.hh"

#include <ecs/Components.hh>
#include <ecs/Ecs.hh>

namespace ecs {
	DoorSystem::DoorSystem(ecs::EntityManager &em) : entities(em) {}

	bool DoorSystem::Frame(float dtSinceLastFrame) {
		for (Entity ent : entities.EntitiesWith<SlideDoor, SignalReceiver>()) {
			auto doorRead = *ent.Get<SlideDoor>();

			SlideDoor::State state = doorRead.GetState(entities);
			auto receiver = ent.Get<SignalReceiver>();
			auto door = ent.Get<SlideDoor>();
			if (receiver->IsTriggered()) {
				if (state != SlideDoor::State::OPENED && state != SlideDoor::State::OPENING) { door->Open(entities); }
			} else {
				if (state != SlideDoor::State::CLOSED && state != SlideDoor::State::CLOSING) { door->Close(entities); }
			}
		}
		return true;
	}
} // namespace ecs
