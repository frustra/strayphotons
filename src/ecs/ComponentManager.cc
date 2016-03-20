#include "ComponentManager.hh"

template <typename CompType>
virtual CompType *Assign(Entity e, CompType comp)
{

}

template <typename CompType>
virtual void Remove<CompType>(Entity e)
{

}

virtual void RemoveAll(Entity e)
{

}

template <typename CompType>
virtual bool Has<CompType>(Entity e)
{

}

template <typename CompType>
CompType *Get<CompType>(Entity e)
{

}