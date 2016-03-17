#ifndef ENTITY_MANAGER_H
#define ENTITY_MANAGER_H

#include "Shared.hh"

namespace sp {

	class EntityManager
	{
	public:
		Entity NewEntity();

	private:
		vector<Entity> entities;
	}

	class Entity
	{
	public:

		static const uint32 INDEX_BITS = 48;
		static const uint64 INDEX_MASK = (1 << INDEX_BITS) - 1;

		static const uint32 GENERATION_BITS = 16;
		static const uint64 GENERATION_MASK = (1 << GENERATION_BITS) - 1;

		Entity(uint64 index, uint32 generation) {
			assert(index & INDEX_BITS == index);
			assert(generation & GENERATION_BITS == generation);
			id = (generation << INDEX_BITS) + index;
		}

		Entity(uint64 id): id(id) {}

		uint64 Index() const {
			return id & INDEX_MASK;
		}

		uint32 Generation() const {
			return (id >> INDEX_BITS) & GENERATION_MASK;
		}

	private:
		uint64 id;
	}
}


#endif