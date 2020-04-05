#include "assets/AssetManager.hh"
#include "assets/Asset.hh"

namespace sp
{
	Asset::Asset(AssetManager *manager, const std::string &path) : manager(manager), path(path)
	{
	}

	Asset::~Asset()
	{
		manager->Unregister(*this);
		delete[] buffer;
	}

	inline const uint8 *Asset::Buffer()
	{
		if (buffer == nullptr)
		{
			std::lock_guard<std::mutex> lock(readLock);
			// Check again in case another thread loaded it while we were waiting.
			if (buffer == nullptr)
			{
				if (input.is_open() || InputStream(input, &size))
				{
					sizeValid = true;
					input.seekg(0, std::ios::beg);
					uint8_t *read_buffer = new uint8[size];
					input.read(reinterpret_cast<char *>(read_buffer), size);
					input.close();
					buffer = read_buffer;
				}
			}
		}
		return buffer;
	}

	const char *Asset::CharBuffer()
	{
		return reinterpret_cast<const char *>(Buffer());
	}

	inline const std::string Asset::String()
	{
		return std::string(CharBuffer(), Size());
	}

	inline int Asset::Size()
	{
		Buffer(); // Call Buffer to ensure the buffer is loaded
		return size;
	}
}

