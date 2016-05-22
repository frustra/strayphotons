#ifndef SP_ASSETMANAGER_H
#define SP_ASSETMANAGER_H

#include "Common.hh"
#include "tiny_gltf_loader.h"

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

		void Unregister(const Asset &asset);

	private:
		std::string base;
		AssetMap loadedAssets;
		ModelMap loadedModels;

		tinygltf::TinyGLTFLoader gltfLoader;
	};
}

#endif

