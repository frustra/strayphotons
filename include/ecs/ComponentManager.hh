#ifndef COMPONENT_MANAGER_H
#define COMPONENT_MANAGER_H

#include <typeindex>
#include <unordered_map>
#include <bitset>

#include "Common.hh"
#include "ecs/Entity.hh"
#include "ecs/ComponentStorage.hh"

#define MAX_COMPONENT_TYPES 64

namespace sp
{
	class ComponentManager
	{
		// TODO-cs: should probably just merge these two classes
		friend class EntityManager;
	public:
		typedef std::bitset<MAX_COMPONENT_TYPES> ComponentMask;

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		template <typename CompType, typename ...T>
		CompType *Assign(Entity e, T... args);

		template <typename CompType>
		void Remove(Entity e);

		void RemoveAll(Entity e);

		template <typename CompType>
		bool Has(Entity e) const;

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		template <typename CompType>
		CompType *Get(Entity e);

		size_t ComponentTypeCount() const;

	private:

		template <typename ...CompTypes>
		ComponentMask createMask();

		template <typename TypeId>
		ComponentMask &setMask(ComponentManager::ComponentMask &mask, const TypeId &stdTypeId);

		template <typename TypeId, typename ...OtherTypeIds>
		ComponentMask &setMask(ComponentManager::ComponentMask &mask, const TypeId &stdTypeId, const OtherTypeIds &... stdTypeIds);

		// it is REALLY a vector of ComponentPool<T>* where each pool is the storage
		// for a different type of component.  I'm not sure of a type-safe way to store
		// this while still allowing dynamic addition of new component types.
		vector<void *> componentPools;

		// map the typeid(T) of a component type, T, to the "index" of that
		// component type. Any time each component type stores info in a vector, this index
		// will identify which component type it corresponds to
		std::unordered_map<std::type_index, uint32> compTypeToCompIndex;

		// An entity's index gives a bitmask for the components that it has. If bitset[i] is set
		// then it means this entity has the component with component index i
		vector<ComponentMask> entCompMasks;

	};

	template <typename CompType, typename ...T>
	CompType *ComponentManager::Assign(Entity e, T... args)
	{
		std::type_index compType = typeid(CompType);

		uint32 compIndex;

		try
		{
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

		return static_cast<ComponentPool<CompType>*>(componentPools.at(compIndex))->NewComponent(e, args...);
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
				static_cast<BaseComponentPool *>(componentPools.at(i))->Remove(e);
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
	CompType *ComponentManager::Get(Entity e)
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
	ComponentManager::ComponentMask ComponentManager::createMask()
	{
		ComponentManager::ComponentMask mask;
		return setMask(mask, typeid(CompTypes)...);
	}

	template <typename TypeId>
	ComponentManager::ComponentMask &ComponentManager::setMask(ComponentManager::ComponentMask &mask, const TypeId &stdTypeId)
	{
		auto compIndex = compTypeToCompIndex.at(stdTypeId);
		mask.set(compIndex);
		return mask;
	}

	template <typename TypeId, typename ...OtherTypeIds>
	ComponentManager::ComponentMask &ComponentManager::setMask(ComponentManager::ComponentMask &mask,
			const TypeId &stdTypeId, const OtherTypeIds &... stdTypeIds)
	{
		setMask(mask, stdTypeId);
		return setMask(mask, stdTypeIds...);
	}

	size_t ComponentManager::ComponentTypeCount() const
	{
		return componentPools.size();
	}
}

#endif