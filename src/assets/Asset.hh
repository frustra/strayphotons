#pragma once

#include "Common.hh"
#include <mutex>

namespace sp
{
	class AssetManager;

	class Asset : public NonCopyable
	{
	public:
		Asset(AssetManager *manager, const string &path);
		~Asset();

		virtual bool InputStream(std::ifstream &stream, size_t *size = nullptr) = 0;
		virtual bool OutputStream(std::ofstream &stream) = 0;

		inline const uint8 *Buffer();
		inline const char *CharBuffer();
		inline const string String();
		inline int Size();

		AssetManager *manager;

		const string path;

	private:
		uint8 *buffer = nullptr;
		size_t size = 0;
		bool sizeValid = false;
		
		std::ifstream input;
		std::mutex readLock;
	};
}
