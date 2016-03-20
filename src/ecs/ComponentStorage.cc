#include "ComponentStorage.hh"

namespace sp
{
	template <typename CompType>
	CompType *ComponentPool::NewComponent(Entity e)
	{
		int newCompIndex;

		if (components.size() == lastCompIndex + 1)
		{
			// resize instead of push_back(CompType()) to avoid an extra construction of CompType
			newCompIndex = components.size();
			components.resize(components.size() + 1);
		}
		else
		{
			newCompIndex = lastCompIndex;
		}

		entIndexToCompIndex[e.Index()] = newCompIndex;
		lastCompIndex = newCompIndex;

		return &components.at(newCompIndex);
	}

	template <typename CompType>
	CompType *ComponentPool::Get(Entity e)
	{
		if (!HasComponent(e))
		{
			return nullptr;
		}

		compIndex = entIndexToCompIndex.at(e.Index());
		return comonents.at(compIndex);
	}

	template <typename CompType>
	void ComponentPool::Remove(Entity e)
	{
		if (!HasComponent(e))
		{
			throw runtime_error("cannot remove component because the entity does not have one");
		}

		// Swap this component to the end and decrement the container last index

		// TODO-cs: swap is done via copy instead of via move.  Investigate performance difference
		// if this is changed.  My guess is that caching won't work as well if move is used but it will
		// make removing a component quicker (avoids two copy operations)
		CompType temp = components.at(lastCompIndex);

		removeIndex = entIndexToCompIndex.at(e.Index());
		components.at(lastCompIndex) = components.at(removeIndex);
		components.at(removingIndex) = temp;

		entIndexToCompIndex.at(compIndexToEntIndex.at(lastCompIndex)) = removeIndex;
		entIndexToCompIndex.at(e.Index()) = INVALID_COMP_INDEX;
		lastCompIndex--;
	}

	template <typename CompType>
	bool ComponentPool::HasComponent(Entity e)
	{
		auto compIndex = entIndexToCompIndex.find(e.Index());
		return (compIndex != entIndexToCompIndex.end()) && compIndex->second != INVALID_COMP_INDEX);
	}
}

