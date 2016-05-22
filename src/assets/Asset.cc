#include "assets/AssetManager.hh"
#include "assets/Asset.hh"

namespace sp
{
	Asset::Asset(AssetManager *manager, const std::string &path, char *buffer, size_t size) : manager(manager), path(path), buffer(buffer), size(size)
	{
	}

	Asset::~Asset()
	{
		manager->Unregister(*this);
	}

	const std::string Asset::String()
	{
		return std::string(buffer);
	}

	const char *Asset::Buffer()
	{
		return buffer;
	}

	int Asset::Size()
	{
		return size;
	}
}

