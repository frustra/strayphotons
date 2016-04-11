#ifndef ENTITY_H
#define ENTITY_H

#include <ostream>

namespace sp
{
	class EntityManager;

	class Entity
	{
		friend class EntityManager;
	public:
		// the rest of the bits are for the generation
		static const uint32 INDEX_BITS = 48;
		static const uint64 INDEX_MASK = ((uint64)1 << INDEX_BITS) - 1;

		Entity() : Entity(NULL_ID) {};
		Entity(const Entity &) = default;
		Entity &operator=(const Entity &) & = default;

		uint64 Index() const
		{
			return id & INDEX_MASK;
		}

		uint64 Generation() const
		{
			return (id >> INDEX_BITS);
		}

		bool operator==(const Entity &other)
		{
			return id == other.id;
		}
		bool operator!=(const Entity &other)
		{
			return !(*this == other);
		}

		friend std::ostream &operator<<(std::ostream &os, const Entity e)
		{
			os << "(Index: " << e.Index() << ", Gen: " << e.Generation() << ")";
			return os;
		}


	private:
		Entity(uint64 index, uint16 generation)
		{
			id = (static_cast<uint64>(generation) << INDEX_BITS) + index;
			Assert((id & INDEX_MASK) == index);
		}

		Entity(uint64 id): id(id) {}

		static const uint64 NULL_ID = 0;

		uint64 id;
	};

}
#endif