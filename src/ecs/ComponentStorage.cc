#include <stdexcept>
#include "ecs/ComponentStorage.hh"
#include "ecs/Entity.hh"


namespace sp
{
	template <typename CompType>
	ComponentPool<CompType>::ComponentPool()
	{
		lastCompIndex = reinterpret_cast<uint64>(-1);
		softRemoveMode = false;
	}

	template <typename CompType>
	CompType *ComponentPool<CompType>::NewComponent(Entity e)
	{
		int newCompIndex = lastCompIndex + 1;
		lastCompIndex = newCompIndex;

		if (components.size() == newCompIndex)
		{
			// resize instead of push_back() to avoid an extra construction of CompType
			components.resize(components.size() + 1);
		}

		components.at(newCompIndex).first = e;
		entIndexToCompIndex[e.Index()] = newCompIndex;

		return &components.at(newCompIndex);
	}

	template <typename CompType>
	CompType *ComponentPool<CompType>::Get(Entity e)
	{
		if (!HasComponent(e))
		{
			return nullptr;
		}

		uint64 compIndex = entIndexToCompIndex.at(e.Index());
		return components.at(compIndex);
	}

	template <typename CompType>
	void ComponentPool<CompType>::Remove(Entity e)
	{
		if (!HasComponent(e))
		{
			throw std::runtime_error("cannot remove component because the entity does not have one");
		}

		entIndexToCompIndex.at(e.Index()) = ComponentPool<CompType>::INVALID_COMP_INDEX;

		uint64 removeIndex = entIndexToCompIndex.at(e.Index());

		if (softRemoveMode)
		{
			softRemove(removeIndex);
		}
		else
		{
			remove(removeIndex);
		}
	}

	template <typename CompType>
	void ComponentPool<CompType>::remove(uint64 compIndex)
	{
		// Swap this component to the end and decrement the container last index

		// TODO-cs: swap is done via copy instead of via move.  Investigate performance difference
		// if this is changed.  My guess is that caching won't work as well if move is used but it will
		// make removing a component quicker (avoids two copy operations)
		CompType temp = components.at(lastCompIndex);
		components.at(lastCompIndex) = components.at(compIndex);
		components.at(compIndex) = temp;

		lastCompIndex--;
	}

	template <typename CompType>
	void ComponentPool<CompType>::softRemove(uint64 compIndex)
	{
		// mark the component as the "Null" Entity and add this component index to queue of
		// components to be deleted when "soft remove" mode is disabled.
		// "Null" Entities will never be iterated over
		Assert(compIndex < components.size());

		components[compIndex].first = Entity::Null();
		softRemoveCompIndexes.push(compIndex);
	}

	template <typename CompType>
	bool ComponentPool<CompType>::HasComponent(Entity e) const
	{
		auto compIndex = entIndexToCompIndex.find(e.Index());
		return compIndex != entIndexToCompIndex.end() && compIndex->second != ComponentPool<CompType>::INVALID_COMP_INDEX;
	}

	template <typename CompType>
	size_t ComponentPool<CompType>::Size() const
	{
		return lastCompIndex + 1;
	}

	template <typename CompType>
	void ComponentPool<CompType>::toggleSoftRemove(bool enabled)
	{
		if (enabled)
		{
			if (softRemoveMode)
			{
				throw runtime_error("soft remove mode is already active");
			}
		}
		else
		{
			if (!softRemoveMode)
			{
				throw runtime_error("soft remove mode is already inactive");
			}

			// must perform proper removes for everything that has been "soft removed"
			while (!softRemoveCompIndexes.empty())
			{
				uint64 compIndex = softRemoveCompIndexes.front();
				softRemoveCompIndexes.pop();
				remove(compIndex);
			}
		}

		softRemoveMode = enabled;
	}

	template <typename CompType>
	Entity ComponentPool<CompType>::entityAt(uint64 compIndex)
	{
		Assert(compIndex < components.size());
		return components[compIndex].first;
	}

	BaseComponentPool::IterateLock::IterateLock(BaseComponentPool &pool): pool(pool)
	{
		pool.toggleSoftRemove(true);
	}

	BaseComponentPool::IterateLock::~IterateLock()
	{
		pool.toggleSoftRemove(false);
	}

	ComponentPoolEntityCollection::ComponentPoolEntityCollection(BaseComponentPool &pool)
		: pool(pool)
	{
		// keep track of the last component at creation time.  This way, if new components
		// are created during iteration they will be added at the end and we will not iterate over them
		lastCompIndex = pool.Size() - 1;
	}

	ComponentPoolEntityCollection::Iterator ComponentPoolEntityCollection::begin()
	{
		return ComponentPoolEntityCollection::Iterator(pool, 0);
	}

	ComponentPoolEntityCollection::Iterator ComponentPoolEntityCollection::end()
	{
		return ComponentPoolEntityCollection::Iterator(pool, lastCompIndex + 1);
	}

	ComponentPoolEntityCollection::Iterator::Iterator(BaseComponentPool &pool, uint64 compIndex)
		: pool(pool), compIndex(compIndex)
	{}

	ComponentPoolEntityCollection::Iterator &ComponentPoolEntityCollection::Iterator::operator++()
	{
		compIndex++;
		return *this;
	}

	Entity ComponentPoolEntityCollection::Iterator::operator*()
	{
		Assert(compIndex < pool.Size());
		return pool.entityAt(compIndex);
	}

	template <typename CompType>
	ComponentPoolEntityCollection &&ComponentPool<CompType>::Entities()
	{
		return std::move(ComponentPoolEntityCollection(this));
	}

}

