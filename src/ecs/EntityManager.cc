#include "EntityManager.hh"

Entity EntityManager::NewEntity()
void EntityManager::Destroy(Entity e);
bool EntityManager::Alive(Entity e);

template <typename CompType>
virtual void EntityManager::Assign(Entity e, CompType comp);

template <typename CompType>
void EntityManager::Remove<CompType>(Entity e);

void EntityManager::RemoveAllComponents(Entity e);

template <typename CompType>
bool EntityManager::Has<CompType>(Entity e);

template <typename CompType>
weak_ptr<CompType> EntityManager::Get<CompType>(Entity e);

template <typename CompType...>
Entity EntityManager::EachWith(CompType comp...);