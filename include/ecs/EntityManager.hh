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
		 * be iterated over before starting iteration.  The reason why is as follows:
		 * during iteration, if any entity that **WOULD** have caused a callback is disqualified
		 * for that callback (because an earlier callback removed one of its components) then
		 * it will **NOT** trigger a callback.  The number of entities iterated over could change
		 * depending on the internal ordering of the components at the start of iteration.
		 *
		 * Ex: the following snippet shows how to call a lambda function for each entity
		 *     that has "Comp1" and "Comp5" components:
		 *
		 *     EachWith([](Entity e, Comp1 *c1, Comp5 *c5) {
		 *         cout << "Entity " << e << " has C1 x " << c1->x << " and C5 y " << c5->y;
		 *     })
		 */
		blah compile failure for attention:
		// TODO-cs: implement "soft delete" of components when a component pool is being iterated over
		// by adding their indices to a "delete later" queue instead of swapping the components to the back.
		// this prevents iteration from missing some valid components that were swapped from the back when
		// deletions ocurred.  After iteration, this "delete later" queue is drained by performing an actual
		// "swap delete" on the components.  Toggling this mode must be done from within the EachWith function
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