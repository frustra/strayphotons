#ifndef ENTITY_MANAGER_IMPL_H
#define ENTITY_MANAGER_IMPL_H

#include "ecs/EntityManager.hh"
#include "ecs/Entity.hh"

namespace sp
{
	EntityManager::EntityManager()
	{
		// entity 0 is reserved for the NULL Entity
		nextEntityIndex = 1;

		// update data structures for the NULL Entity
		compMgr.entCompMasks.resize(1);
		entIndexToGen.push_back(0);
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

		return Entity(this, Entity::Id(i, gen));
	}

	void EntityManager::Destroy(Entity::Id e)
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

	bool EntityManager::Valid(Entity::Id e) const
	{
		return e.Generation() == entIndexToGen.at(e.Index());
	}

	template <typename CompType, typename ...T>
	CompType *EntityManager::Assign(Entity::Id e, T... args)
	{
		return compMgr.Assign<CompType>(e, args...);
	}

	template <typename CompType>
	void EntityManager::Remove(Entity::Id e)
	{
		compMgr.Remove<CompType>(e);
	}

	void EntityManager::RemoveAllComponents(Entity::Id e)
	{
		compMgr.RemoveAll(e);
	}

	template <typename CompType>
	bool EntityManager::Has(Entity::Id e) const
	{
		return compMgr.Has<CompType>(e);
	}

	template <typename CompType>
	CompType *EntityManager::Get(Entity::Id e)
	{
		return compMgr.Get<CompType>(e);
	}

	EntityManager::EntityCollection EntityManager::EntitiesWith(ComponentManager::ComponentMask compMask)
	{
		// find the smallest size component pool to iterate over
		size_t minSize;
		int minSizeCompIndex = -1;

		for (size_t i = 0; i < compMgr.ComponentTypeCount(); ++i)
		{
			if (!compMask.test(i))
			{
				continue;
			}

			size_t compSize = static_cast<BaseComponentPool *>(compMgr.componentPools.at(i))->Size();

			if (compSize < minSize || minSizeCompIndex == -1)
			{
				minSize = compSize;
				minSizeCompIndex = i;
			}
		}

		auto smallestCompPool = static_cast<BaseComponentPool *>(compMgr.componentPools.at(minSizeCompIndex));

		return EntityManager::EntityCollection(*this, compMask, smallestCompPool->Entities(),
		                                       smallestCompPool->CreateIterateLock());
	}

	template <typename ...CompTypes>
	EntityManager::EntityCollection EntityManager::EntitiesWith()
	{
		return EntitiesWith(compMgr.CreateMask<CompTypes...>());
	}

	EntityManager::EntityCollection::Iterator::Iterator(EntityManager &em,
			const ComponentManager::ComponentMask &compMask,
			ComponentPoolEntityCollection *compEntColl,
			ComponentPoolEntityCollection::Iterator compIt)
		: em(em), compMask(compMask), compEntColl(compEntColl), compIt(compIt)
	{}

	EntityManager::EntityCollection::Iterator &EntityManager::EntityCollection::Iterator::operator++()
	{
		while (++compIt != compEntColl->end())
		{
			Entity::Id e = *compIt;
			auto entCompMask = em.compMgr.entCompMasks.at(e.Index());
			if ((entCompMask & compMask).any())
			{
				break;
			}
		}
		return *this;
	}

	bool EntityManager::EntityCollection::Iterator::operator==(Iterator &other)
	{
		return compMask == other.compMask && compIt == other.compIt;
	}
	bool EntityManager::EntityCollection::Iterator::operator!=(Iterator &other)
	{
		return !(*this == other);
	}

	Entity EntityManager::EntityCollection::Iterator::operator*()
	{
		return Entity(&this->em, *compIt);
	}

	EntityManager::EntityCollection::EntityCollection(EntityManager &em,
			const ComponentManager::ComponentMask &compMask,
			ComponentPoolEntityCollection compEntColl,
			unique_ptr<BaseComponentPool::IterateLock> &&iLock)
		: em(em), compMask(compMask), compEntColl(compEntColl), iLock(std::move(iLock))
	{}

	EntityManager::EntityCollection::Iterator EntityManager::EntityCollection::begin()
	{
		return EntityManager::EntityCollection::Iterator(em, compMask, &compEntColl, compEntColl.begin());
	}

	EntityManager::EntityCollection::Iterator EntityManager::EntityCollection::end()
	{
		return EntityManager::EntityCollection::Iterator(em, compMask, &compEntColl, compEntColl.end());
	}

	template <typename ...CompTypes>
	ComponentManager::ComponentMask EntityManager::CreateComponentMask()
	{
		return compMgr.CreateMask<CompTypes...>();
	}

	template <typename ...CompTypes>
	ComponentManager::ComponentMask &EntityManager::SetComponentMask(ComponentManager::ComponentMask &mask)
	{
		return compMgr.SetMask<CompTypes...>(mask);
	}
}

#endif