#ifndef SP_ASSET_H
#define SP_ASSET_H

#include "Common.hh"

namespace sp
{
	class AssetManager;

	class Asset
	{
	public:
		Asset(AssetManager *manager, const string &path);
		~Asset();

		void Load();

		const string String();
		const char *Buffer();
		int Size();

		AssetManager *manager;

		const string path;

	private:
		char *buffer = nullptr;
		size_t size = 0;

		Asset(const Asset &other);
		Asset &operator= (const Asset &other);
	};
}

#endif

