#ifndef ENTITY_MANAGER_H
#define ENTITY_MANAGER_H

#include <queue>
#include <iostream>
#include "Shared.hh"
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
		virtual CompType* Assign<CompType>(Entity e, T... args);

		template <typename CompType>
		void Remove<CompType>(Entity e);

		void RemoveAllComponents(Entity e);

		template <typename CompType>
		bool Has<CompType>(Entity e) const;

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		template <typename CompType>
		CompType *Get<CompType>(Entity e);

		template <typename CompType...>
		Entity EachWith(CompType comp...);

	private:

		static const RECYCLE_ENTITY_COUNT = 2048;

		vector<Entity> entities;
		vector<uint16> entIndexToGen;
		std::queue<uint64> freeEntityIndexes;
		uint64 nextEntityIndex;
		unique_ptr<ComponentManagerInterface> compMgr;
	};
}


#endif