#pragma once

#include <typeindex>
#include <unordered_map>
#include <bitset>
#include <sstream>

#include "Common.hh"
#include "ecs/Entity.hh"
#include "ecs/ComponentStorage.hh"
#include "ecs/UnrecognizedComponentType.hh"
#include "ecs/Handle.hh"

#define MAX_COMPONENT_TYPES 64

namespace ecs
{
	class ComponentManager
	{
		// TODO-cs: should probably just merge these two classes
		friend class EntityManager;
	public:
		typedef std::bitset<MAX_COMPONENT_TYPES> ComponentMask;

		template <typename CompType, typename ...T>
		Handle<CompType> Assign(Entity::Id e, T... args);

		template <typename CompType>
		void Remove(Entity::Id e);

		void RemoveAll(Entity::Id e);

		template <typename CompType>
		bool Has(Entity::Id e) const;

		template <typename CompType>
		Handle<CompType> Get(Entity::Id e);

		size_t ComponentTypeCount() const
		{
			return componentPools.size();
		}

		template <typename ...CompTypes>
		ComponentMask &SetMask(ComponentMask &mask);

		template <typename ...CompTypes>
		ComponentMask CreateMask();

		/**
		 * Register the given type as a valid "Component type" in the system.
		 * This means that operations to search for entities with this component
		 * as well as checking if an entity has that component will not fail with
		 * an exception.
		 *
		 * It is good practice to do this with all intended component types during
		 * program initialization to prevent errors when checking for component types
		 * that have yet to be assigned to an entity.
		 *
		 * This will throw an std::runtime_error if any of the types are already registered.
		 */
		template<typename CompType>
		void RegisterComponentType();

	private:
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
	Handle<CompType> ComponentManager::Assign(Entity::Id e, T... args)
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
			RegisterComponentType<CompType>();
			compIndex = compTypeToCompIndex.at(compType);
		}

		sp::Assert(entCompMasks.size() > e.Index(), "entity does not have a component mask");

		auto &compMask = entCompMasks.at(e.Index());
		compMask.set(compIndex);

		auto componentPool = static_cast<ComponentPool<CompType>*>(componentPools.at(compIndex));
		componentPool->NewComponent(e, args...);
		return Handle<CompType>(e, componentPool);
	}

	template <typename CompType>
	void ComponentManager::Remove(Entity::Id e)
	{
		std::type_index tIndex = typeid(CompType);
		if (compTypeToCompIndex.count(tIndex) == 0)
		{
			throw UnrecognizedComponentType(tIndex);
		}

		uint32 compIndex = compTypeToCompIndex.at(tIndex);

		auto &compMask = entCompMasks.at(e.Index());
		if (compMask[compIndex] == false)
		{
			throw runtime_error("entity does not have a component of type "
				+ string(tIndex.name()));
		}

		static_cast<ComponentPool<CompType>*>(componentPools.at(compIndex))->Remove(e);
		compMask.reset(compIndex);
	}

	template <typename CompType>
	bool ComponentManager::Has(Entity::Id e) const
	{
		std::type_index compType = typeid(CompType);
		if (compTypeToCompIndex.count(compType) == 0)
		{
			throw UnrecognizedComponentType(compType);
		}

		auto compIndex = compTypeToCompIndex.at(compType);
		return entCompMasks.at(e.Index())[compIndex];
	}

	template <typename CompType>
	Handle<CompType> ComponentManager::Get(Entity::Id e)
	{
		if (!Has<CompType>(e))
		{
			throw runtime_error("entity does not have a component of type "
				+ string(typeid(CompType).name()));
		}

		auto compIndex = compTypeToCompIndex.at(typeid(CompType));
		auto *compPool = static_cast<ComponentPool<CompType>*>(componentPools.at(compIndex));
		return Handle<CompType>(e, compPool);
	}

	template <typename CompType>
	void ComponentManager::RegisterComponentType()
	{
		std::type_index compType = typeid(CompType);

		if (compTypeToCompIndex.count(compType) != 0)
		{
			std::stringstream ss;
			ss << "component type " << string(compType.name()) << " is already registered";
			throw std::runtime_error(ss.str());
		}

		uint32 compIndex = componentPools.size();
		compTypeToCompIndex[compType] = compIndex;
		componentPools.push_back(new ComponentPool<CompType>());
	}

	template <typename ...CompTypes>
	ComponentManager::ComponentMask ComponentManager::CreateMask()
	{
		ComponentManager::ComponentMask mask;
		if (sizeof...(CompTypes) == 0)
		{
			return mask;
		}
		return SetMask<CompTypes...>(mask);
	}

	template <typename ...CompTypes>
	ComponentManager::ComponentMask &ComponentManager::SetMask(ComponentMask &mask)
	{
		return setMask(mask, std::type_index(typeid(CompTypes))...);
	}

	template <typename TypeId>
	ComponentManager::ComponentMask &ComponentManager::setMask(ComponentMask &mask, const TypeId &stdTypeId)
	{
		if (compTypeToCompIndex.count(stdTypeId) == 0)
		{
			throw invalid_argument(string(stdTypeId.name()) + " is an invalid component type, it is unknown to the system.");
		}
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
}
