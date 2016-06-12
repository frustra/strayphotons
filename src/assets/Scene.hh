#pragma once

#include "Common.hh"
#include "ecs/Ecs.hh"

#include <unordered_map>

namespace sp
{
	class Asset;

	class Scene : public NonCopyable
	{
	public:
		Scene(const string &name, shared_ptr<Asset> asset);
		~Scene() {}

		const string name;
		vector<Entity> entities;
		std::unordered_map<string, Entity> namedEntities;

		Entity FindEntity(const std::string name);
	private:
		shared_ptr<Asset> asset;
	};
}
