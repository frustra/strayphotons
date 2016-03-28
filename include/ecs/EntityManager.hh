#ifndef ENTITY_MANAGER_H
#define ENTITY_MANAGER_H

#include <queue>
#include <iostream>
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

		// Fn must be a function that accepts ...CompType args
		template <typename Fn, typename ...CompType>
		Entity EachWith(Fn fn, CompType*... comp);

	private:

		static const size_t RECYCLE_ENTITY_COUNT = 2048;

		vector<Entity> entities;
		vector<uint16> entIndexToGen;
		std::queue<uint64> freeEntityIndexes;
		uint64 nextEntityIndex;
		unique_ptr<ComponentManager> compMgr;
	};
}


#endif