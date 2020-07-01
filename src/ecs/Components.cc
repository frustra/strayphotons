#include <ecs/Components.hh>

namespace ecs
{
	typedef std::map<std::string, ComponentBase *> ComponentList;
	ComponentList *GComponentList;

	void RegisterComponent(const char *name, ComponentBase *comp)
	{
		if (GComponentList == nullptr) GComponentList = new ComponentList();
		if (GComponentList->count(name) > 0) throw std::runtime_error("Duplicate component registration: " + std::string(name));
		GComponentList->emplace(name, comp);
	}


	void RegisterComponents(EntityManager &em)
	{
		em.RegisterKeyedComponentType<ecs::Name>();
		if (GComponentList == nullptr) GComponentList = new ComponentList();
		for (auto comp : *GComponentList)
		{
			comp.second->Register(em);
		}
	}

	ComponentBase *LookupComponent(const std::string name)
	{
		if (GComponentList == nullptr) GComponentList = new ComponentList();

		try
		{
			return GComponentList->at(name);
		}
		catch (std::out_of_range)
		{
			return nullptr;
		}
	}
}
