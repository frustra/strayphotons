#include <stdexcept>
#include <sstream>
#include "ecs/EntityManager.hh"

namespace sp
{
	EntityManager::EntityManager()
	{
		// entity 0 is reserved for the NULL Entity
		nextEntityIndex = 1;

		// create a blank mask for the NULL Entity
		// TODO-cs: EntityManager and ComponentManager really should just be one class...
		compMgr.entCompMasks.resize(1);
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
			Assert(compMgr.entCompMasks[i] == ComponentManager::ComponentMask(),
				   "expected ent comp mask to be reset at destruction but it wasn't");
			compMgr.entCompMasks[i] = ComponentManager::ComponentMask();
		}
		else
		{
			i = nextEntityIndex++;
			gen = 0;
			entIndexToGen.push_back(gen);

			// add a blank comp mask without copying one in
			compMgr.entCompMasks.resize(compMgr.entCompMasks.size() + 1);

			Assert(entIndexToGen.size() == nextEntityIndex);
			Assert(compMgr.entCompMasks.size() == nextEntityIndex);
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
	CompType *EntityManager::Assign(Entity e, T... args)
	{
		return compMgr.Assign<CompType>(e, args...);
	}

	template <typename CompType>
	void EntityManager::Remove(Entity e)
	{
		compMgr.Remove<CompType>(e);
	}

	void EntityManager::RemoveAllComponents(Entity e)
	{
		compMgr.RemoveAll(e);
	}

	template <typename CompType>
	bool EntityManager::Has(Entity e) const
	{
		return compMgr.Has<CompType>(e);
	}

	template <typename CompType>
	CompType *EntityManager::Get(Entity e)
	{
		return compMgr.Get<CompType>(e);
	}

	template <typename ReturnT, typename ...CompTypes>
	Entity EntityManager::EachWith(std::function<ReturnT(Entity, CompTypes *...)> callback)
	{
		if (sizeof...(CompTypes) == 0)
		{
			throw invalid_argument("EachWith must be called with at least one component type specified");
		}

		auto compMask = compMgr.createMask<CompTypes...>();

		// identify the component type which has the least number of entities to iterate over
		size_t minSize = -1;
		int minSizeCompIndex = -1;

		for (size_t i = 0; i < compMgr.ComponentTypeCount(); ++i)
		{
			if (!compMask.test(i))
			{
				continue;
			}

			size_t compSize = static_cast<BaseComponentPool *>(compMgr.componentPools.at(i))->Size();

			if ((int)compSize < minSize || minSizeCompIndex == -1)
			{
				minSize = compSize;
				minSizeCompIndex = i;
			}
		}

		// when iterating over this pool we must enable "soft deletes" so that
		// if any components are deleted they do not affect the ordering of any of the other
		// components in this pool (normally deletions are a swap-to-back operation)
		BaseComponentPool *smallestCompPool = compMgr.componentPools.at(minSizeCompIndex);
		smallestCompPool->toggleSoftRemove(true);

		// For every entity in the smallest common pool, callback if the entity has all the components
		for (Entity e : smallestCompPool->Entities())
		{
			if (compMgr.entCompMasks.at(e.Index()) & compMask == compMask)
			{
				callback(e, (compMgr.Get<CompTypes>(e))...);
			}
		}


	}
}