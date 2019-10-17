#pragma once

#include "Ecs.hh"
#include "core/Logging.hh"

namespace ecs
{
	class NamedEntity
	{
	public:
		NamedEntity() {}
		NamedEntity(std::string name) : name(name) {}
		NamedEntity(Entity &ent, std::string name = "") : name(name), ent(ent) {}

		std::string Name() const
		{
			return name;
		}

		NamedEntity &Load(EntityManager &em)
		{
			if (!this->name.empty() && !this->ent.Valid())
			{
				// TODO: Find entity in ECS
				// ecs::Entity parent = scene->FindEntity(subTransform.second.get<string>());
				// if (!parent.Valid())
				// {
				// 	throw std::runtime_error("Entity relative to non-existent parent");
				// }
				// transform->SetParent(parent);
				std::stringstream ss;
				ss << *this;
				Errorf("Entity does not exist: %s", ss.str());
				this->name = "";
			}
			return *this;
		}

		NamedEntity &operator=(const std::string &other)
		{
			this->name = other;
			return *this;
		}

		NamedEntity &operator=(const Entity &other)
		{
			this->ent = other;
			return *this;
		}

		bool operator==(const Entity &other) const
		{
			return this->ent == other;
		}

		bool operator!=(const Entity &other) const
		{
			return this->ent != other;
		}

		Entity &operator*()
		{
			return ent;
		}

		Entity *operator->()
		{
			return &ent;
		}

		bool operator!() const
		{
			return !ent.Valid();
		}

		operator bool() const
		{
			return ent.Valid();
		}

		friend std::ostream &operator<<(std::ostream &os, const NamedEntity e)
		{
			if (!e.name.empty())
			{
				os << "NamedEntity(" << e.name << ")";
			}
			else if (e.ent.Valid())
			{
				os << "Entity(" << e << ")";
			}
			else
			{
				os << "Entity(NULL)";
			}
			return os;
		}

	private:
		std::string name = "";
		Entity ent;
	};
}
