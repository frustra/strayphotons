#ifndef COMPONENT_MANAGER_H
#define COMPONENT_MANAGER_H

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
		vector<ComponentPool> componentPools;
	};
}

#endif