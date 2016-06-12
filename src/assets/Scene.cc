#include "assets/Asset.hh"
#include "assets/Scene.hh"

namespace sp
{
	Scene::Scene(const string &name, shared_ptr<Asset> asset) : name(name), asset(asset)
	{
	}

	Entity Scene::FindEntity(const std::string name)
	{
		if (namedEntities.count(name)) return namedEntities[name];
		return Entity();
	}
}
