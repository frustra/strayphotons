#ifndef COMPONENT_MANAGER_H
#define COMPONENT_MANAGER_H

#include <typeindex>
#include <unordered_map>
#include <bitset>

#include "Common.hh"
#include "ecs/Entity.hh"
#include "ecs/ComponentStorage.hh"

#define MAX_COMPONENT_TYPES 64

namespace sp
{
	class ComponentManager
	{
		// TODO-cs: should probably just merge these two classes
		friend class EntityManager;
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
		typedef ComponentMask std::bitset<MAX_COMPONENT_TYPES>;

		template <typename ...CompTypes>
		ComponentMask createMask() const;

		template <typnename FirstComp, typename ...OtherComps>
		ComponentMask setMask(ComponentMask &mask);

		template <typename CompType>
		ComponentMask setMask(ComponentMask &mask);

		// it is REALLY a vector of ComponentPool<T>* where each pool is the storage
		// for a different type of component.  I'm not sure of a type-safe way to store
		// this while still allowing dynamic addition of new component types.
		vector<void*> componentPools;

		// map the typeid(T) of a component type, T, to the "index" of that
		// component type. Any time each component type stores info in a vector, this index
		// will identify which component type it corresponds to
		std::unordered_map<std::type_index, uint32> compTypeToCompIndex;

		// An entity's index gives a bitmask for the components that it has. If bitset[i] is set
		// then it means this entity has the component with component index i
		vector<CompMask> entCompMasks;

	};
}

#endif