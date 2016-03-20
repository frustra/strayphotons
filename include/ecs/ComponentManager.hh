#ifndef COMPONENT_MANAGER_H
#define COMPONENT_MANAGER_H

namespace sp
{
	class ComponentManagerInterface
	{
	public:
		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		template <typename CompType>
		virtual CompType *Assign<CompType>(Entity e) = 0;

		template <typename CompType>
		virtual void Remove<CompType>(Entity e) = 0;

		virtual void RemoveAll(Entity e) = 0;

		template <typename CompType>
		virtual bool Has<CompType>(Entity e) = 0;

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		template <typename CompType>
		CompType *Get<CompType>(Entity e);
	}

	class ComponentManager : public ComponentManagerInterface
	{
	public:
		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		template <typename CompType>
		virtual CompType *Assign(Entity e, CompType comp);

		template <typename CompType>
		virtual void Remove<CompType>(Entity e);

		virtual void RemoveAll(Entity e);

		template <typename CompType>
		virtual bool Has<CompType>(Entity e);

		// DO NOT CACHE THIS POINTER, a component's pointer may change over time
		template <typename CompType>
		CompType *Get<CompType>(Entity e);

	private:
		vector<ComponentPool> componentPools;
	}
}

#endif