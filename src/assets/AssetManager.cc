#define TINYGLTF_LOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "tiny_gltf_loader.h"

#include "assets/AssetManager.hh"
#include "assets/Asset.hh"
#include "assets/Model.hh"

#include "core/Logging.hh"

#include <iostream>
#include <fstream>

namespace sp
{
	const std::string ASSETS_DIR = "../assets/";

	shared_ptr<Asset> AssetManager::Load(const std::string &path)
	{
		Debugf("Loading asset: %s", path.c_str());
		AssetMap::iterator it = loadedAssets.find(path);
		shared_ptr<Asset> asset;

		if (it == loadedAssets.end())
		{
			std::ifstream in(ASSETS_DIR + path, std::ios::in | std::ios::binary);

			if (in)
			{
				in.seekg(0, std::ios::end);
				size_t size = in.tellg();
				char *buffer = new char[size];
				in.seekg(0, std::ios::beg);
				in.read(buffer, size);
				in.close();

				asset = make_shared<Asset>(this, path, buffer, size);
				loadedAssets[path] = weak_ptr<Asset>(asset);
			}
			else
			{
				throw "Invalid asset path";
			}
		}
		else
		{
			asset = it->second.lock();
		}

		return asset;
	}

	shared_ptr<Model> AssetManager::LoadModel(const std::string &name)
	{
		Debugf("Loading model: %s", name.c_str());
		ModelMap::iterator it = loadedModels.find(name);
		shared_ptr<Model> model;

		if (it == loadedModels.end())
		{
			shared_ptr<Asset> asset = Load("models/" + name + "/" + name + ".gltf");
			model = make_shared<Model>(name, asset);
			std::string err;

			bool ret = gltfLoader.LoadASCIIFromString(&model->scene, &err, asset->Buffer(), asset->Size(), ASSETS_DIR + "models/" + name);
			if (!err.empty())
			{
				throw err.c_str();
			}
			else if (!ret)
			{
				throw "Failed to parse glTF";
			}

			loadedModels[name] = weak_ptr<Model>(model);
		}
		else
		{
			model = it->second.lock();
		}

		return model;
	}

	void AssetManager::Unregister(const Asset &asset)
	{
		loadedAssets.erase(asset.path);
	}
}

