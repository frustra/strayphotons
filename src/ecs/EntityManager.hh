#pragma once

#include <queue>
#include <iostream>
#include <functional>
#include <stdexcept>
#include <sstream>
#include "Common.hh"
#include "ecs/ComponentManager.hh"
#include "ecs/Entity.hh"
#include "ecs/Handle.hh"


namespace sp
{
	class EntityManager
	{
	public:
		class EntityCollection
		{
		public:
			class Iterator : public std::iterator<std::input_iterator_tag, Entity>
			{
			public:
				Iterator(EntityManager &em, const ComponentManager::ComponentMask &compMask,
						 ComponentPoolEntityCollection *compEntColl, ComponentPoolEntityCollection::Iterator compIt);
				Iterator &operator++();
				bool operator==(Iterator &other);
				bool operator!=(Iterator &other);
				Entity operator*();
			private:
				EntityManager &em;
				const ComponentManager::ComponentMask &compMask;
				ComponentPoolEntityCollection *compEntColl;
				ComponentPoolEntityCollection::Iterator compIt;
			};

			// An IterateLock on compEntColl's component pool is needed so that
			// if any components are deleted they do not affect the ordering of any of the other
			// components in this pool (normally deletions are a swap-to-back operation)
			// this lock releases on destructions so it's okay if Exceptions are raised when iterating
			EntityCollection(EntityManager &em, const ComponentManager::ComponentMask &compMask,
							 ComponentPoolEntityCollection compEntColl,
							 unique_ptr<BaseComponentPool::IterateLock> &&iLock);
			Iterator begin();
			Iterator end();
		private:
			EntityManager &em;
			ComponentManager::ComponentMask compMask;
			ComponentPoolEntityCollection compEntColl;
			unique_ptr<BaseComponentPool::IterateLock> iLock;
		};

		EntityManager();

		/**
		 * Add a new entity to the ECS.
		 */
		Entity NewEntity();

		/**
		 * Remove the given entity from the ECS.
		 * THIS MAKES NO GUARENTEE to call destructors on components that were assigned
		 * to this entity.
		 */
		void Destroy(Entity::Id e);

		/**
		 * Check if the given entity is still valid in the system.
		 * An entity is invalid if it is not currently present in the system.
		 * Since Entity::Id and Entity objects are often passed by value it is useful
		 * to check if the underlying entity they represent is still valid before operating on them.
		 */
		bool Valid(Entity::Id e) const;

		/**
		 * Construct a new component of type "CompType" with the given arguments
		 * and attach it to the given entity.
		 *
		 * Returns a handle to the created component.
		 */
		template <typename CompType, typename ...T>
		Handle<CompType> Assign(Entity::Id e, T... args);

		/**
		 * Remove the component of type "CompType" from the given entity
		 */
		template <typename CompType>
		void Remove(Entity::Id e);

		/**
		 * Remove all components that the given entity has
		 */
		void RemoveAllComponents(Entity::Id e);

		/**
		 * Check if an entity has a given type of component
		 */
		template <typename CompType>
		bool Has(Entity::Id e) const;

		/**
		 * Get a handle to an entity's component of type "CompType"
		 */
		template <typename CompType>
		Handle<CompType> Get(Entity::Id e);

		/**
		 * Register the given type as a valid "Component type" in the system.
		 * This means that operations to search for entities with this component
		 * as well as checking if an entity has that component will not fail with
		 * an exception.
		 *
		 * It is good practice to do this with all intended component types during
		 * program initialization to prevent errors when checking for component types
		 * that have yet to be assigned to an entity.
		 */
		template<typename CompType>
		void RegisterComponentType();

		/**
		 * Create a component mask for the given types
		 */
		template <typename ...CompTypes>
		ComponentManager::ComponentMask CreateComponentMask();

		/**
		 * Set the flags for the given component types on the given mask
		 */
		template <typename ...CompTypes>
		ComponentManager::ComponentMask &SetComponentMask(ComponentManager::ComponentMask &mask);

		/**
		 * Used to iterate over all entities that have the given components.
		 *
		 * Ex Usage:
		 * ```
		 * for (Entity e : entMgr.EntitiesWith<CompType1, CompType8>())
		 * {
		 *      // use e
		 * }
		 * ```
		 * The entities iterated over are a snapshot of what was in the system when this
		 * method was called; if you create new entities that would qualify for iterating over
		 * after calling this method (during iteration) then this will not be iterated over.
		 *
		 * If you will be removing components from the same set of entities that are being
		 * iterated over then you **CANNOT** depend on how many of these components will actually
		 * be iterated over before starting iteration; if A deletes B and A is iterated to first then
		 * B will not trigger a callback but if B is iterated to before A then both will trigger callbacks.
		 */
		template <typename ...CompTypes>
		EntityCollection EntitiesWith();

		/**
		 * Same as the template form of this function except that the components
		 * are speicified with the given ComponentMask.  This is usually created with
		 * "CreateComponentMask" and possibly "SetComponentMask" methods
		 */
		EntityCollection EntitiesWith(ComponentManager::ComponentMask compMask);

	private:

		static const size_t RECYCLE_ENTITY_COUNT = 2048;

		vector<Entity::Id> entities;
		vector<uint16> entIndexToGen;
		std::queue<uint64> freeEntityIndexes;
		uint64 nextEntityIndex;
		ComponentManager compMgr;
	};
};
