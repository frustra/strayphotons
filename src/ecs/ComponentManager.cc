#include "ecs/ComponentManager.hh"

namespace ecs
{
	void ComponentManager::RemoveAll(Entity::Id e)
	{
		sp::Assert(entCompMasks.size() > e.Index(), "entity does not have a component mask");

		auto &compMask = entCompMasks.at(e.Index());
		for (uint64 i = 0; i < componentPools.size(); ++i)
		{
			if (compMask[i])
			{
				static_cast<BaseComponentPool *>(componentPools.at(i))->Remove(e);
				compMask.reset(i);
			}
		}

		sp::Assert(compMask == ComponentMask(),
				   "component mask not blank after removing all components");
	}
}
