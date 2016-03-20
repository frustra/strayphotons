#include "assets/AssetManager.hh"
#include "assets/Asset.hh"

#include <iostream>
#include <fstream>

namespace sp
{
	Asset::Asset(AssetManager *manager, const std::string &path) : manager(manager), path(path)
	{
	}

	Asset::~Asset()
	{
		manager->Unregister(*this);
	}

	void Asset::Load()
	{
		std::ifstream in("assets/" + path, std::ios::in | std::ios::binary);

		if (in)
		{
			in.seekg(0, std::ios::end);
			size = in.tellg();
			buffer = new char[size];
			in.seekg(0, std::ios::beg);
			in.read(buffer, size);
			in.close();
		}
		else
		{
			throw "Invalid asset path";
		}
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

