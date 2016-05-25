#ifndef ENTITY_MANAGER_IMPL_H
#define ENTITY_MANAGER_IMPL_H

#include "ecs/EntityManager.hh"
#include "ecs/Entity.hh"

namespace sp
{
	template <typename CompType, typename ...T>
	CompType *EntityManager::Assign(Entity::Id e, T... args)
	{
		return compMgr.Assign<CompType>(e, args...);
	}

	template <typename CompType>
	void EntityManager::Remove(Entity::Id e)
	{
		compMgr.Remove<CompType>(e);
	}

	template <typename CompType>
	bool EntityManager::Has(Entity::Id e) const
	{
		return compMgr.Has<CompType>(e);
	}

	template <typename CompType>
	CompType *EntityManager::Get(Entity::Id e)
	{
		return compMgr.Get<CompType>(e);
	}

	template <typename ...CompTypes>
	EntityManager::EntityCollection EntityManager::EntitiesWith()
	{
		return EntitiesWith(compMgr.CreateMask<CompTypes...>());
	}

	template <typename ...CompTypes>
	ComponentManager::ComponentMask EntityManager::CreateComponentMask()
	{
		return compMgr.CreateMask<CompTypes...>();
	}

	template <typename ...CompTypes>
	ComponentManager::ComponentMask &EntityManager::SetComponentMask(ComponentManager::ComponentMask &mask)
	{
		return compMgr.SetMask<CompTypes...>(mask);
	}
}

#endif