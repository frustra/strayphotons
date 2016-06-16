#pragma once

#include "ecs/ComponentStorage.hh"
#include "ecs/Entity.hh"

namespace sp
{
	/**
	 * Handles are meant as a substitute for component pointers/references
	 * since the actual memory address of a component may change at many
	 * different times without the user of the ECS expecting it.
	 *
	 * Usage is similar to a pointer.
	 */
	template <typename CompType>
	class Handle
	{
	public:
		Handle(Entity::Id entityId, ComponentPool<CompType> *componentPool);
		~Handle();

		CompType & operator*() const;
		CompType * operator->() const;

	private:
		Entity::Id eId;
		ComponentPool<CompType> *compPool;
	};

	template <typename CompType>
	Handle<CompType>::Handle(Entity::Id entityId, ComponentPool<CompType> *componentPool)
		: eId(entityId), compPool(componentPool)
	{
	}

	template <typename CompType>
	Handle<CompType>::~Handle()
	{
	}

	template <typename CompType>
	CompType & Handle<CompType>::operator*() const
	{
		return *compPool->Get(eId);
	}

	template <typename CompType>
	CompType * Handle<CompType>::operator->() const
	{
		return compPool->Get(eId);
	}
}

