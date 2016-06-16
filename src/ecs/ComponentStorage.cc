#include "ecs/ComponentStorage.hh"

namespace ecs
{
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

	Entity::Id ComponentPoolEntityCollection::Iterator::operator*()
	{
		sp::Assert(compIndex < pool.Size());
		return pool.entityAt(compIndex);
	}
}
