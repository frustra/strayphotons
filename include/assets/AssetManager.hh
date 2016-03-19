#ifndef SP_ASSETMANAGER_H
#define SP_ASSETMANAGER_H

#include "Shared.hh"

#include <unordered_map>
#include <string>
#include <map>

namespace sp
{
	class Asset;

	typedef std::unordered_map<std::string, weak_ptr<Asset> > AssetMap;

	class AssetManager
	{
	public:
		shared_ptr<Asset> Load(const std::string &path);

		void Unregister(const Asset &asset);

	private:
		std::string base;
		AssetMap loadedAssets;
	};
}

#endif

