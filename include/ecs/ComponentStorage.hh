#ifndef COMPONENT_STORAGE_H
#define COMPONENT_STORAGE_H

#include <unordered_map>
#include "Shared.hh"
#include "ecs/Entity.hh"


namespace sp
{
	/**
	 * ComponentPool is a storage container for Entity components.
	 * It stores all components with a vector and so it only grows when new components are added.
	 * It "recycles" and keeps storage unfragmented by swapping components to the end when they are removed
	 * but does not guarantee the internal ordering of components based on insertion or Entity index.
	 * It allows efficient iteration since there are no holes in its component storage.
	 *
	 * TODO-cs: an incremental allocator instead of a vector will be better when there are lots of
	 * components being added; this should be implemeted.
	 */
	template <typename CompType>
	class ComponentPool
	{
	public:
		ComponentPool();

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		CompType *NewComponent(Entity e);

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		CompType *Get(Entity e);
		void Remove(Entity e);
		bool HasComponent(Entity e) const;

	private:
		vector<CompType> components;
		uint64 lastCompIndex;
		std::unordered_map<uint64, uint64> entIndexToCompIndex;
		std::unordered_map<uint64, uint64> compIndexToEntIndex;
	};
}

#endif