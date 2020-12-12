#include "NetworkComponentSystem.hh"

#include <ecs/EcsImpl.hh>

namespace ecs {
    NetworkComponentSystem::NetworkComponentSystem(EntityManager &entities) : entities(entities) {}
    NetworkComponentSystem::~NetworkComponentSystem() {}

} // namespace ecs
