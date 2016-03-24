#include <stdexcept>
#include <sstream>
#include "ecs/EntityManager.hh"

namespace sp
{
	EntityManager::EntityManager()
	{
		nextEntityIndex = 1;
		compMgr = unique_ptr<ComponentManager>(new ComponentManager());
	}

	Entity EntityManager::NewEntity()
	{
		uint64 i;
		uint16 gen;
		if (freeEntityIndexes.size() >= RECYCLE_ENTITY_COUNT)
		{
			i = freeEntityIndexes.front();
			freeEntityIndexes.pop();
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
			ss << "entity " << e << " is not valid; it may have already been destroyed.";
			throw std::invalid_argument(ss.str());
		}

		RemoveAllComponents(e);
		entIndexToGen.at(e.Index())++;
		freeEntityIndexes.push(e.Index());
	}

	bool EntityManager::Valid(Entity e) const
	{
		return e.Generation() == entIndexToGen.at(e.Index());
	}

	template <typename CompType, typename ...T>
	CompType* EntityManager::Assign(Entity e, T... args)
	{
		return compMgr->Assign<CompType>(e, args...);
	}

	template <typename CompType>
	void EntityManager::Remove(Entity e)
	{
		compMgr->Remove<CompType>(e);
	}

	void EntityManager::RemoveAllComponents(Entity e)
	{
		compMgr->RemoveAll(e);
	}

	template <typename CompType>
	bool EntityManager::Has(Entity e) const
	{
		return compMgr->Has<CompType>(e);
	}

	template <typename CompType>
	CompType* EntityManager::Get(Entity e)
	{
		return compMgr->Get<CompType>(e);
	}

	template <typename ...CompType>
	Entity EntityManager::EachWith(CompType*... comp)
	{

	}
}