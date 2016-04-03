#ifndef ENTITY_MANAGER_H
#define ENTITY_MANAGER_H

#include <queue>
#include <iostream>
#include <functional>
#include "Common.hh"
#include "ecs/ComponentManager.hh"
#include "ecs/Entity.hh"


namespace sp
{
	class EntityManager
	{
	public:
		EntityManager();

		Entity NewEntity();
		void Destroy(Entity e);
		bool Valid(Entity e) const;

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		template <typename CompType, typename ...T>
		CompType* Assign(Entity e, T... args);

		template <typename CompType>
		void Remove(Entity e);

		void RemoveAllComponents(Entity e);

		template <typename CompType>
		bool Has(Entity e) const;

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		template <typename CompType>
		CompType *Get(Entity e);

		/**
		 * Call a callback function for each entity that has the given components.
		 * The component ptrs will be populated with the entity's components when
		 * the callback occurs.
		 *
		 * The types of the args in the given callback function specify that only entities
		 * which have all of these components should trigger a callback.
		 *
		 * During iteration, if any new entities qualify to have the callback called on them
		 * (because an earlier callback caused them to gain the right component types) they
		 * will not be iterated over in the same pass.
		 *
		 * If you will be removing components from the same set of entities that are being
		 * iterated over then you **CANNOT** depend on how many of these components will actually
		 * be iterated over before starting iteration; if A deletes B and A is iterated to first then
		 * B will not trigger a callback but if B is iterated to before A then both will trigger callbacks.
		 *
		 * Ex: the following snippet shows how to call a lambda function for each entity
		 *     that has "Comp1" and "Comp5" components:
		 *
		 *     EachWith([](Entity e, Comp1 *c1, Comp5 *c5) {
		 *         cout << "Entity " << e << " has C1 x " << c1->x << " and C5 y " << c5->y;
		 *     })
		 */
		template <typename ReturnT, typename ...CompTypes>
		Entity EachWith(std::function<ReturnT(Entity, CompTypes*...)> callback);

	private:

		static const size_t RECYCLE_ENTITY_COUNT = 2048;

		vector<Entity> entities;
		vector<uint16> entIndexToGen;
		std::queue<uint64> freeEntityIndexes;
		uint64 nextEntityIndex;
		ComponentManager compMgr;
	};
}


#endif