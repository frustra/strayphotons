//=== Copyright Frustra Software, all rights reserved ===//

#ifndef SP_ASSET_H
#define SP_ASSET_H

#include "Shared.hh"

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
		char *buffer;
		size_t size;

		Asset(const Asset &other);
		Asset &operator= (const Asset &other);
	};
}

#endif

