#include "NetworkComponentSystem.hh"

#include <ecs/components/Network.hh>
#include <ecs/components/Transform.hh>

namespace ecs {
	NetworkComponentSystem::NetworkComponentSystem(EntityManager &entities) : entities(entities) {}
	NetworkComponentSystem::~NetworkComponentSystem() {}

} // namespace ecs
