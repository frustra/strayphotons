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

	void Entity::RemoveAllComponents()
	{
		em->RemoveAllComponents(this->eid);
	}
}
