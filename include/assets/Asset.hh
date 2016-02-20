//=== Copyright Frustra Software, all rights reserved ===//

#ifndef SP_ASSET_H
#define SP_ASSET_H

#include <string>

namespace sp
{
	class AssetManager;

	class Asset
	{
	public:
		Asset(AssetManager *manager, const std::string &path);
		~Asset();

		void Load();

		const std::string String();
		const char *Buffer();
		int Size();

		AssetManager *manager;

		const std::string path;

	private:
		char *buffer;
		int size;

		Asset(const Asset &other);
		Asset &operator= (const Asset &other);
	};
}

#endif

