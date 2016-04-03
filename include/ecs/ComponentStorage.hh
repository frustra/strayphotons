#ifndef COMPONENT_STORAGE_H
#define COMPONENT_STORAGE_H

#include <unordered_map>
#include <bitset>
#include <queue>
#include <iterator>
#include "Common.hh"
#include "ecs/Entity.hh"

#define MAX_COMPONENTS 64

namespace sp
{
	class BaseComponentPool;
	// class BaseComponentPool::IterateLock;

	// Object to capture the state of entities at a given time.  Used for iterating over a
	// ComponentPool's Entities
	class ComponentPoolEntityCollection
	{
	public:
		class Iterator : public std::iterator<std::input_iterator_tag, Entity>
		{
		public:
			Iterator(BaseComponentPool &pool, uint64 compIndex);
			Iterator& operator++();
			Entity operator*();
		private:
			BaseComponentPool &pool;
			uint64 compIndex;
		};

		ComponentPoolEntityCollection(BaseComponentPool &pool);
		Iterator begin();
		Iterator end();
	private:
		BaseComponentPool &pool;
		uint64 lastCompIndex;
	};

	// These classes are meant to be internal to the ECS system and should not be known
	// to code outside of the ECS system
	class BaseComponentPool
	{
		friend class ComponentPoolEntityCollection::Iterator;
	public:
		/**
		* Creating this lock will enable "soft remove" mode on the given ComponentPool.
		* The destruction of this lock will re-enable normal deletion mode.
		*/
		class IterateLock : public NonCopyable
		{
			friend BaseComponentPool;
		public:
			~IterateLock();
		private:
			IterateLock(BaseComponentPool& pool);

			BaseComponentPool &pool;
		};
		friend IterateLock;

		virtual ~BaseComponentPool() {}

		virtual void Remove(Entity e) = 0;
		virtual bool HasComponent(Entity e) const = 0;
		virtual size_t Size() const = 0;
		virtual ComponentPoolEntityCollection&& Entities() = 0;

		// as long as the resultant lock is not destroyed, the order that iteration occurs
		// over the components must stay the same.
		virtual IterateLock CreateIterateLock() = 0;

	private:
		// when toggleSoftRemove(true) is called then any Remove(e) calls
		// must guarentee to not alter the internal ordering of components.
		// When toggleSoftRemove(false) is called later then removals are allowed to rearrange
		// internal ordering again
		virtual void toggleSoftRemove(bool enabled) = 0;

		// method used by ComponentPoolEntityCollection::Iterator to find the next Entity
		virtual Entity entityAt(uint64 compIndex) = 0;

	};

	/**
	 * ComponentPool is a storage container for Entity components.
	 * It stores all components with a vector and so it only grows when new components are added.
	 * It "recycles" and keeps storage unfragmented by swapping components to the end when they are removed
	 * but does not guarantee the internal ordering of components based on insertion or Entity index.
	 * It allows efficient iteration since there are no holes in its component storage.
	 *
	 * TODO-cs: an incremental allocator instead of a vector will be better once the number
	 * components is very large; this should be implemeted.
	 */
	template <typename CompType>
	class ComponentPool : public BaseComponentPool
	{

	public:
		ComponentPool();

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		CompType *NewComponent(Entity e);

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		CompType *Get(Entity e);
		void Remove(Entity e) override;
		bool HasComponent(Entity e) const override;
		size_t Size() const override;
		ComponentPoolEntityCollection&& Entities() override;

		BaseComponentPool::IterateLock&& CreateIterateLock() override;

	private:
		vector<std::pair<Entity, CompType> > components;
		uint64 lastCompIndex;
		std::unordered_map<uint64, uint64> entIndexToCompIndex;
		bool softRemoveMode;
		std::queue<uint64> softRemoveCompIndexes;

		void toggleSoftRemove(bool enabled) override;

		void softRemove(uint64 compIndex);
		void remove(uint64 compIndex);

		Entity entityAt(uint64 compIndex) override;
	};
}

#endif