#ifndef ENTITY_IMPL_H
#define ENTITY_IMPL_H

#include "ecs/Entity.hh"
#include "ecs/EntityManager.hh"

namespace sp
{
	void Entity::Destroy()
	{
		em->Destroy(this->eid);
	}

	bool Entity::Valid() const
	{
		return em->Valid(this->eid);
	}

	template <typename CompType, typename ...T>
	CompType *Entity::Assign(T... args)
	{
		return em->Assign<CompType>(this->eid, args...);
	}

	template <typename CompType>
	void Entity::Remove()
	{
		em->Remove<CompType>(this->eid);
	}

	void Entity::RemoveAllComponents()
	{
		em->RemoveAllComponents(this->eid);
	}

	template <typename CompType>
	bool Entity::Has() const
	{
		return em->Has<CompType>(this->eid);
	}

	template <typename CompType>
	CompType * Entity::Get()
	{
		return em->Get<CompType>(this->eid);
	}
}

namespace std
{
	template <>
	class hash<sp::Entity>
	{
	public:
		size_t operator()(const sp::Entity &e) const
		{
			return hash<uint64>()(e.eid.id);
		}
	};
}

#endif