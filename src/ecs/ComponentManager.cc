#include "ecs/ComponentManager.hh"

namespace sp
{
	template <typename CompType, typename ...T>
	CompType* ComponentManager::Assign(Entity e, T... args)
	{
		std::type_index compType = typeid(CompType);

		uint32 compIndex;

		try {
			compIndex = compTypeToCompIndex.at(compType);
		}
		// component never seen before, add it to the collection
		catch (const std::out_of_range &e)
		{
			compIndex = componentPools.size();
			compTypeToCompIndex[compType] = compIndex;
			componentPools.push_back(new ComponentPool<CompType>());
		}

		Assert(entCompMasks.size() > e.Index(), "entity does not have a component mask");

		auto &compMask = entCompMasks.at(e.Index());
		compMask.set(compIndex);

		return static_cast<ComponentPool<CompType>*>(componentPools.at(compIndex))->NewComponent(e);
	}

	template <typename CompType>
	void ComponentManager::Remove(Entity e)
	{
		std::type_index tIndex = typeid(CompType);
		if (compTypeToCompIndex.count(tIndex) == 0)
		{
			throw invalid_argument("template is not a component type; it has never been added to the system");
		}

		uint32 compIndex = compTypeToCompIndex.at(tIndex);

		auto &compMask = entCompMasks.at(e.Index());
		if (compMask[compIndex] == false)
		{
			throw runtime_error("entity does not have this type of component");
		}

		static_cast<ComponentPool<CompType>*>(componentPools.at(compIndex))->Remove(e);
		compMask.reset(compIndex);
	}

	void ComponentManager::RemoveAll(Entity e)
	{
		Assert(entCompMasks.size() > e.Index(), "entity does not have a component mask");

		auto &compMask = entCompMasks.at(e.Index());
		for (uint i = 0; i < componentPools.size(); ++i)
		{
			if (compMask[i])
			{
				static_cast<BaseComponentPool*>(componentPools.at(i))->Remove(e);
				compMask.reset(i);
			}
		}

		Assert(compMask == ComponentMask(),
			"component mask not blank after removing all components");
	}

	template <typename CompType>
	bool ComponentManager::Has(Entity e) const
	{
		std::type_index compType = typeid(CompType);
		if (compTypeToCompIndex.count(compType) == 0)
		{
			throw invalid_argument("template is not a component type; it has never been added to the system");
		}

		auto compIndex = compTypeToCompIndex.at(compType);
		return entCompMasks.at(e.Index())[compIndex];
	}

	template <typename CompType>
	CompType* ComponentManager::Get(Entity e)
	{
		uint32 compIndex = compTypeToCompIndex.at(typeid(CompType));
		const auto &entCompMask = entCompMasks.at(e.Index());
		if (entCompMask[compIndex] == false)
		{
			throw runtime_error("entity does not have this type of component");
		}

		return static_cast<ComponentPool<CompType>*>(componentPools.at(compIndex))->Get(e);
	}

	template <typename ...CompTypes>
	ComponentManager::ComponentMask ComponentManager::createMask() const
	{
		ComponentManager::ComponentMask mask;
		return setMask<CompTypes...>(mask);
	}

	template <typename FirstComp, typename ...OtherComps>
	ComponentManager::ComponentMask ComponentManager::setMask(ComponentManager::ComponentMask &mask)
	{
		setMask<FirstComp>(mask);
		return setMask<OtherComps...>(mask);
	}

	template <typename CompType>
	ComponentManager::ComponentMask ComponentManager::setMask(ComponentManager::ComponentMask &mask)
	{
		auto compIndex = compTypeToCompIndex.at(typeid(CompType));
		mask.set(compIndex);
		return mask;
	}

	size_t ComponentManager::ComponentTypeCount() const
	{
		return componentPools.size();
	}
}

