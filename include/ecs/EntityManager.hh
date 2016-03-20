#ifndef ENTITY_MANAGER_H
#define ENTITY_MANAGER_H

#include "Shared.hh"

namespace sp
{

	class EntityManager
	{
	public:
		Entity NewEntity();
		void Destroy(Entity e);
		bool Valid(Entity e);

		template <typename CompType>
		virtual void Assign(Entity e, CompType comp);

		template <typename CompType>
		void Remove<CompType>(Entity e);

		void RemoveAllComponents(Entity e);

		template <typename CompType>
		bool Has<CompType>(Entity e);

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		template <typename CompType>
		CompType *Get<CompType>(Entity e);

		template <typename CompType...>
		Entity EachWith(CompType comp...);

	private:
		vector<Entity> entities;
		ComponentManager compMgr;
	}

	class Entity
	{
	public:

		static const uint32 INDEX_BITS = 48;
		static const uint64 INDEX_MASK = (1 << INDEX_BITS) - 1;

		static const uint32 GENERATION_BITS = 16;
		static const uint64 GENERATION_MASK = (1 << GENERATION_BITS) - 1;

		bool operator==(const Entity &other)
		{
			return id == other.id
		}
		bool operator!=(const Entity &other)
		{
			return !(*this == other)
		}

		Entity(uint64 index, uint32 generation)
		{
			assert(index & INDEX_BITS == index);
			assert(generation & GENERATION_BITS == generation);
			id = (generation << INDEX_BITS) + index;
		}

		Entity(uint64 id): id(id) {}

		uint64 Index() const
		{
			return id & INDEX_MASK;
		}

		uint32 Generation() const
		{
			return (id >> INDEX_BITS) & GENERATION_MASK;
		}

	private:
		uint64 id;
	}
}


#endif