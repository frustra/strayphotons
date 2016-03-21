#include <stdexcept>
#include "ecs/EntityManager.hh"

namespace sp
{
	EntityManager::EntityManager()
	{
		nextEntityIndex = 1;
		compMgr = make_unique<ComponentManager>();
	}

	Entity EntityManager::NewEntity()
	{
		uint64 i;
		uint16 gen;
		if (freeEntityIndexes.size() >= RECYCLE_ENTITY_COUNT)
		{
			i = freeEntityIndexes.pop_front();
			gen = entIndexToGen.at(i);  // incremented at Entity destruction
		}
		else
		{
			i = nextEntityIndex++;
			gen = 0;
			entIndexToGen.push_back(gen);
		}

		return Entity(i, gen);
	}

	void EntityManager::Destroy(Entity e)
	{
		if (!Valid(e))
		{
			std::stringstream ss;
			ss << "entity " << e << " is not valid; it may have already been destroyed."
			throw std::invalid_argument(ss.c_str());
		}
		entIndexToGen.at(e.Index())++;
	}

	bool EntityManager::Valid(Entity e) const
	{
		return e.Generation() == entIndexToGen.at(e.Index());
	}

	template <typename CompType, T...>
	virtual CompType* EntityManager::Assign<CompType>(Entity e, T... args)
	{
		return compMgr->Assign<CompType>(e, args...);
	}

	template <typename CompType>
	void EntityManager::Destroy<CompType>(Entity e)
	{
		compMgr->Remove<CompType>(e);
	}

	void EntityManager::RemoveAllComponents(Entity e)
	{
		compMgr->RemoveAll(e);
	}

	template <typename CompType>
	bool EntityManager::Has<CompType>(Entity e) const
	{
		return compMgr->Has<CompType>(e);
	}

	template <typename CompType>
	CompType* EntityManager::Get<CompType>(Entity e)
	{
		return compMgr->Get<CompType>(e);
	}

	template <typename CompType...>
	Entity EntityManager::EachWith(CompType comp...)
	{

	}
}