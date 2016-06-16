#pragma once

#include <ostream>
#include "Common.hh"

namespace ECS
{
	class EntityManager;

	template<typename CompType>
	class Handle;

	/**
	 * Convenience class for operations on entities instead of
	 * going through the EntityManager
	 */
	class Entity
	{
	public:
		friend struct std::hash<Entity>;
		class Id
		{
			friend struct std::hash<Entity>;
			friend class EntityManager;
		public:

			// the rest of the bits are for the generation
			static const uint32 INDEX_BITS = 48;
			static const uint64 INDEX_MASK = ((uint64)1 << INDEX_BITS) - 1;

			Id() : Id(NULL_ID) {};
			Id(const Id &) = default;
			Id &operator=(const Id &) & = default;

			uint64 Index() const
			{
				return id & INDEX_MASK;
			}

			uint64 Generation() const
			{
				return (id >> INDEX_BITS);
			}

			bool operator==(const Id &other) const
			{
				return id == other.id;
			}
			bool operator!=(const Id &other) const
			{
				return !(*this == other);
			}
			bool operator<(const Id &other) const
			{
				return this->id < other.id;
			}

			friend std::ostream &operator<<(std::ostream &os, const Id e)
			{
				os << "(Index: " << e.Index() << ", Gen: " << e.Generation() << ")";
				return os;
			}

		private:
			Id(uint64 index, uint16 generation)
			{
				id = (static_cast<uint64>(generation) << INDEX_BITS) + index;
				sp::Assert((id & INDEX_MASK) == index);
			}

			Id(uint64 id): id(id) {}

			static const uint64 NULL_ID = 0;

			uint64 id;
		};

		Entity(const Entity &) = default;
		Entity &operator=(const Entity &) & = default;

		bool operator==(const Entity &other) const
		{
			return this->eid == other.eid && this->em == other.em;
		}
		bool operator!=(const Entity &other) const
		{
			return !(*this == other);
		}
		bool operator<(const Entity &other) const
		{
			return this->eid < other.eid;
		}

		friend std::ostream &operator<<(std::ostream &os, const Entity e)
		{
			os << e.eid;
			return os;
		}

		Entity() : em(nullptr), eid() {}
		Entity(EntityManager *em, Entity::Id eid) : em(em), eid(eid) {}

		EntityManager *GetManager();
		Id GetId() const;

		// retrieves the unique identifier for this entity
		uint64 Index() const
		{
			return eid.Index();
		}

		// Wrappers for EntityManager functions
		void Destroy();
		bool Valid() const;

		template <typename CompType, typename ...T>
		Handle<CompType> Assign(T... args);

		template <typename CompType>
		void Remove();

		void RemoveAllComponents();

		template <typename CompType>
		bool Has() const;

		template <typename CompType>
		Handle<CompType> Get();

	private:
		EntityManager *em;
		Entity::Id eid;
	};
}
