#ifndef ENTITY_H
#define ENTITY_H

#include <cassert>
#include <ostream>

namespace sp
{
	class EntityManager;

	class Entity
	{
		friend class EntityManager;
	public:

		static const uint32 INDEX_BITS = 48;
		static const uint64 INDEX_MASK = ((uint64)1 << INDEX_BITS) - 1;

		static const uint32 GENERATION_BITS = 16;
		static const uint64 GENERATION_MASK = ((uint64)1 << GENERATION_BITS) - 1;

		uint64 Index() const
		{
			return id & INDEX_MASK;
		}

		uint16 Generation() const
		{
			return (id >> INDEX_BITS) & GENERATION_MASK;
		}

		bool operator==(const Entity &other)
		{
			return id == other.id;
		}
		bool operator!=(const Entity &other)
		{
			return !(*this == other);
		}

		friend std::ostream& operator<<(std::ostream& os, const Entity e)
		{
			os << "(Index: " << e.Index() << ", Gen: " << e.Generation() + ")";
			return os;
		}

	private:
		Entity(uint64 index, uint16 generation)
		{
			assert((index & INDEX_BITS) == index);
			assert((generation & GENERATION_BITS) == generation);
			id = (static_cast<uint64>(generation) << INDEX_BITS) + index;
		}

		Entity(uint64 id): id(id) {}

		static const uint64 NULL_ID = 0;

		uint64 id;
	};

}
#endif