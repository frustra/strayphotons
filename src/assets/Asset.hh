#pragma once

#include "Common.hh"

namespace sp
{
	class AssetManager;

	class Asset : public NonCopyable
	{
	public:
		Asset(AssetManager *manager, const string &path, char *buffer, size_t size);
		~Asset();

		const string String();
		const char *Buffer();
		int Size();

		AssetManager *manager;

		const string path;

	private:
		char *buffer = nullptr;
		size_t size = 0;
	};
}
