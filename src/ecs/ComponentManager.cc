#include "ecs/ComponentManager.hh"

namespace sp
{
	template <typename CompType, typename ...T>
	CompType* ComponentManager::Assign(Entity e, T... args)
	{
		std::type_index typeIndex = typeid(CompType);

		uint32 poolIndex;
		try {
			poolIndex = compTypeToPoolIndex.at(typeIndex);
		}
		catch (const std::out_of_range &e)
		{
			poolIndex = componentPools.size();
			compTypeToPoolIndex[typeIndex] = poolIndex;
			componentPools.push_back(new ComponentPool<CompType>());
		}

		return static_cast<ComponentPool<CompType>*>(componentPools.at(poolIndex))->NewComponent(e);
	}

	template <typename CompType>
	void ComponentManager::Remove(Entity e)
	{

	}

	void ComponentManager::RemoveAll(Entity e)
	{

	}

	template <typename CompType>
	bool ComponentManager::Has(Entity e) const
	{

	}

	template <typename CompType>
	CompType* ComponentManager::Get(Entity e)
	{

	}
}

