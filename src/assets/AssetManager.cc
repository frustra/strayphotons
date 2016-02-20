//=== Copyright Frustra Software, all rights reserved ===//

#include "assets/AssetManager.hh"
#include "assets/Asset.hh"

namespace sp
{
	shared_ptr<Asset> AssetManager::Load(const std::string &path)
	{
		AssetMap::iterator it = loadedAssets.find(path);
		shared_ptr<Asset> asset;

		if (it == loadedAssets.end())
		{
			asset = make_shared<Asset>(this, path);
			loadedAssets[path] = weak_ptr<Asset>(asset);
			asset->Load();
		}
		else
		{
			asset = it->second.lock();
		}

		return asset;
	}

	void AssetManager::Unregister(const Asset &asset)
	{
		loadedAssets.erase(asset.path);
	}
}

