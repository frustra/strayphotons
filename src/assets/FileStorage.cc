extern "C"
{
#include <microtar.h>
}

#include "assets/Storage.hh"
#include "assets/Asset.hh"
#include "assets/AssetHelpers.hh"
#include "assets/Model.hh"
#include "assets/Scene.hh"

#include "core/Logging.hh"
#include "Common.hh"

#include <Ecs.hh>
#include "ecs/Components.hh"

#if !(__APPLE__)
#include <filesystem>
#endif
#include <iostream>
#include <fstream>
#include <utility>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>

namespace sp
{
	void TarStorage::ReloadTarIndex()
	{
		mtar_t tar;
		if (mtar_open(&tar, ASSETS_TAR.c_str(), "r") != MTAR_ESUCCESS)
		{
			Errorf("Failed to build asset index");
			return;
		}

		mtar_header_t h;
		while (mtar_read_header(&tar, &h) != MTAR_ENULLRECORD)
		{
			size_t offset = tar.pos + 512 * sizeof(unsigned char);
			tarIndex[h.name] = std::make_pair(offset, h.size);
			mtar_next(&tar);
		}

		mtar_close(&tar);
	}

	bool AssetManager::ReadWholeFile(std::vector<unsigned char> *out, std::string *err,
                   					 const std::string &path, void * /* user_data */ )
	{
		std::ifstream stream;

#ifdef PACKAGE_RELEASE
		if (tarIndex.size() == 0) UpdateTarIndex();

		stream.open(ASSETS_TAR, std::ios::in | std::ios::binary);

		if (stream && tarIndex.count(path))
		{
			// Get the start and end location of the requested file in the tarIndex
			auto indexData = tarIndex[path];
		
			// Move the stream head to the start location of the file in the tarIndex
			stream.seekg(indexData.first, std::ios::beg);

			// Resize the output vector to match the size of the file to be read
			out->resize(indexData.second, '\0');

			// Copy bytes from the file into the output buffer
			stream.read(reinterpret_cast<char *>(&out->at(0)),
						static_cast<std::streamsize>(indexData.second));
			stream.close();
			return true;
		}
#else
		stream.open(path, std::ios::in | std::ios::binary);

		if (stream)
		{
			// Move the head to the end of the file
			stream.seekg(0, std::ios::end);

			// Get the head position (since we're at the end, this is the filesize)
			size_t fileSize = stream.tellg();

			// Move back to the beginning
			stream.seekg(0, std::ios::beg);

			// Make output buffer fit the file
			out->resize(fileSize, '\0');

			// Copy bytes from the file into the output buffer
			stream.read(reinterpret_cast<char *>(&out->at(0)),
						static_cast<std::streamsize>(fileSize));
			stream.close();

			return true;
		}
#endif
		return false;
	}

	bool FileStorage::InputStream(const std::string &path, std::ifstream &stream, size_t *size)
	{
#ifdef PACKAGE_RELEASE
		if (tarIndex.size() == 0) UpdateTarIndex();

		stream.open(ASSETS_TAR, std::ios::in | std::ios::binary);

		if (stream && tarIndex.count(path))
		{
			auto indexData = tarIndex[path];
			if (size) *size = indexData.second;
			stream.seekg(indexData.first, std::ios::beg);
			return true;
		}

		return false;
#else
		string filename = starts_with(path, "D:") ? path : ((starts_with(path, "shaders/") ? SHADERS_DIR : ASSETS_DIR) + path);
		stream.open(filename, std::ios::in | std::ios::binary);

		if (size && stream)
		{
			stream.seekg(0, std::ios::end);
			*size = stream.tellg();
			stream.seekg(0, std::ios::beg);
		}

		return !!stream;
#endif
	}

	bool FileStorage::OutputStream(const std::string &path, std::ofstream &stream)
	{
#if !(__APPLE__)
		std::filesystem::path p(ASSETS_DIR + path);
		std::filesystem::create_directories(p.parent_path());
#endif

		stream.open(ASSETS_DIR + path, std::ios::out | std::ios::binary);
		return !!stream;
	}

	shared_ptr<Asset> AssetManager::Load(const std::string &path)
	{
		AssetMap::iterator it = loadedAssets.find(path);
		shared_ptr<Asset> asset;

		if (it == loadedAssets.end() || it->second.expired())
		{
			std::ifstream in;
			size_t size;

			if (InputStream(path, in, &size))
			{
				uint8 *buffer = new uint8[size];
				in.read((char *) buffer, size);
				in.close();

				asset = make_shared<Asset>(this, path, buffer, size);
				loadedAssets[path] = weak_ptr<Asset>(asset);
			}
			else
			{
				return nullptr;
			}
		}
		else
		{
			asset = it->second.lock();
		}

		return asset;
	}
}
