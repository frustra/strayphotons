#pragma once

#include "Common.hh"
#include "ecs/Ecs.hh"
#include <tiny_gltf_loader.h>

#include <unordered_map>
#include <string>
#include <map>

namespace sp
{
	class Asset;
	class Model;

	typedef std::unordered_map<std::string, weak_ptr<Asset> > AssetMap;
	typedef std::unordered_map<std::string, weak_ptr<Model> > ModelMap;

	class AssetManager
	{
	public:
		shared_ptr<Asset> Load(const std::string &path);
		shared_ptr<Model> LoadModel(const std::string &name);
		vector<Entity> LoadScene(const std::string &name, EntityManager *em);

		void Unregister(const Asset &asset);

	private:
		std::string base;
		AssetMap loadedAssets;
		ModelMap loadedModels;

		tinygltf::TinyGLTFLoader gltfLoader;
	};
}
