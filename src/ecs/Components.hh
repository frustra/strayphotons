#pragma once

#include <Ecs.hh>
#include "ecs/NamedEntity.hh"
#include <map>
#include <string>
#include <cstring>

namespace picojson
{
	class value;
}

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

	void RegisterComponent(const char *name, ComponentBase *comp);
	ComponentBase *LookupComponent(const std::string name);

	template<typename CompType>
	class Component : public ComponentBase
	{
	public:
		Component(const char *name) : ComponentBase(name)
		{
			auto existing = dynamic_cast<Component<CompType> *>(LookupComponent(std::string(name)));
			if (existing == nullptr)
			{
				RegisterComponent(name, this);
			}
			else if (*this != *existing)
			{
				throw std::runtime_error("Duplicate component type registered: " + std::string(name));
			}
		}

		bool LoadEntity(Entity &dst, picojson::value &src) override
		{
			std::cerr << "Calling undefined LoadEntity on type: " << name << std::endl;
			return false;
		}

		bool SaveEntity(picojson::value &dst, Entity &src) override
		{
			std::cerr << "Calling undefined SaveEntity on type: " << name << std::endl;
			return false;
		}

		virtual void Register(EntityManager &em) override
		{
			em.RegisterComponentType<CompType>();
		}

		bool operator==(const Component<CompType> &other) const
		{
			return strcmp(name, other.name) == 0;
		}

		bool operator!=(const Component<CompType> &other) const
		{
			return !(*this == other);
		}
	};

	template<typename KeyType>
	class KeyedComponent : public Component<KeyType>
	{
	public:
		KeyedComponent(const char *name) : ComponentBase(name)
		{
			auto existing = dynamic_cast<KeyedComponent<KeyType> *>(LookupComponent(std::string(name)));
			if (existing == nullptr)
			{
				RegisterComponent(name, this);
			}
			else if (*this != *existing)
			{
				throw std::runtime_error("Duplicate component type registered: " + std::string(name));
			}
		}

		void Register(EntityManager &em) override
		{
			em.RegisterKeyedComponentType<KeyType>();
		}

		bool operator==(const KeyedComponent<KeyType> &other) const
		{
			return strcmp(this->name, other.name) == 0;
		}

		bool operator!=(const KeyedComponent<KeyType> &other) const
		{
			return !(*this == other);
		}
	};

	void RegisterComponents(EntityManager &em);
}
