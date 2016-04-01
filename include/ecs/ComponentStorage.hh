#ifndef COMPONENT_STORAGE_H
#define COMPONENT_STORAGE_H

#include <unordered_map>
#include <bitset>
#include "Common.hh"
#include "ecs/Entity.hh"

#define MAX_COMPONENTS 64

namespace sp
{
	// These classes are meant to be internal to the ECS system and should not be known
	// to code outside of the ECS system
	class BaseComponentPool
	{
	public:
		virtual ~BaseComponentPool() {}

		virtual void Remove(Entity e) = 0;
		virtual size_t Size() const = 0;
		virtual Entities();
	};

	/**
	 * ComponentPool is a storage container for Entity components.
	 * It stores all components with a vector and so it only grows when new components are added.
	 * It "recycles" and keeps storage unfragmented by swapping components to the end when they are removed
	 * but does not guarantee the internal ordering of components based on insertion or Entity index.
	 * It allows efficient iteration since there are no holes in its component storage.
	 *
	 * TODO-cs: an incremental allocator instead of a vector will be better once the number
	 * components is very large; this should be implemeted.
	 */
	template <typename CompType>
	class ComponentPool : public BaseComponentPool
	{
	public:
		ComponentPool();

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		CompType *NewComponent(Entity e);

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		CompType *Get(Entity e);
		void Remove(Entity e) override;
		bool HasComponent(Entity e) const;
		size_t Size() const;

	private:
		vector<std::pair<Entity, CompType> > components;
		uint64 lastCompIndex;
		std::unordered_map<uint64, uint64> entIndexToCompIndex;
	};
}

#endif