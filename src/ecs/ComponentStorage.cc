#include <stdexcept>
#include "ecs/ComponentStorage.hh"


namespace sp
{
	template <typename CompType>
	ComponentPool<CompType>::ComponentPool()
	{
		lastCompIndex = reinterpret_cast<uint64>(-1);
	}

	template <typename CompType>
	CompType* ComponentPool<CompType>::NewComponent(Entity e)
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
	CompType* ComponentPool<CompType>::Get(Entity e)
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

		// Swap this component to the end and decrement the container last index

		// TODO-cs: swap is done via copy instead of via move.  Investigate performance difference
		// if this is changed.  My guess is that caching won't work as well if move is used but it will
		// make removing a component quicker (avoids two copy operations)
		CompType temp = components.at(lastCompIndex);

		uint64 removeIndex = entIndexToCompIndex.at(e.Index());
		components.at(lastCompIndex) = components.at(removeIndex);
		components.at(removeIndex) = temp;

		entIndexToCompIndex.at(e.Index()) = ComponentPool<CompType>::INVALID_COMP_INDEX;
		lastCompIndex--;
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
}

