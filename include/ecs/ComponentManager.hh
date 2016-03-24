#ifndef COMPONENT_MANAGER_H
#define COMPONENT_MANAGER_H

#include <typeindex>
#include <unordered_map>

#include "Shared.hh"
#include "ecs/Entity.hh"
#include "ecs/ComponentStorage.hh"


namespace sp
{
	class ComponentManager
	{
	public:
		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		template <typename CompType, typename ...T>
		CompType* Assign(Entity e, T... args);

		template <typename CompType>
		void Remove(Entity e);

		void RemoveAll(Entity e);

		template <typename CompType>
		bool Has(Entity e) const;

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		template <typename CompType>
		CompType *Get(Entity e);

	private:
		// it is REALLY a vector of ComponentPool<T>* where each pool is the storage
		// for a different type of component.  I'm not sure of a type-safe way to store
		// this while still allowing dynamic addition of new component types.
		vector<void*> componentPools;
		std::unordered_map<std::type_index, uint32> compTypeToPoolIndex;
	};
}

#endif