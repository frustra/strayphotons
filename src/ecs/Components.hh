#pragma once

#include <Ecs.hh>
#include <map>
#include <string>
#include <tinygltfloader/picojson.h>

namespace ecs
{
	class ComponentBase
	{
	public:
		ComponentBase(const char *name) : name(name) {}

		virtual bool LoadEntity(Entity &dst, picojson::value &src) = 0;
		virtual bool SaveEntity(picojson::value &dst, Entity &src) = 0;
		virtual void Register(EntityManager &em) = 0;

		const char *name;
	};

	typedef std::map<std::string, ComponentBase *> ComponentList;

	extern ComponentList GComponentList;

	template<typename CompType>
	class Component : public ComponentBase
	{
	public:
		Component(const char *name) : ComponentBase(name)
		{
			std::cout << "Registering component: " << name << std::endl;
			if (GComponentList.count(name) > 0) throw new std::runtime_error("Duplicate component registration: " + std::string(name));
			GComponentList[name] = this;
		}

		bool LoadEntity(Entity &dst, picojson::value &src)
		{
			return false;
		}

		bool SaveEntity(picojson::value &dst, Entity &src)
		{
			return false;
		}

		void Register(EntityManager &em)
		{
			em.RegisterComponentType<CompType>();
		}
	};

	static inline void RegisterComponents(EntityManager &em)
	{
		for (auto comp : GComponentList)
		{
			comp.second->Register(em);
		}
	}
}
