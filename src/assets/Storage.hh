#pragma once

#include "Common.hh"
#include <Ecs.hh>
#include "physx/PhysxManager.hh"
#include "graphics/Texture.hh"
#include <tiny_gltf.h>
#include <picojson/picojson.h>

#include <unordered_map>
#include <string>
#include <map>

namespace sp
{
	class Asset;
	class Model;
	class Scene;

	typedef std::unordered_map<std::string, std::pair<size_t, size_t> > OffsetLengthIndex;

	class Storage
	{
	public:
		Storage(AssetManager *manager) : manager(manager) {}

		virtual bool InputStream(const std::string &path, std::ifstream &stream, size_t *size = nullptr) = 0;
		virtual bool OutputStream(const std::string &path, std::ofstream &stream) = 0;

		virtual shared_ptr<Asset> Load(const std::string &path) = 0;
	private:
		AssetManager *manager;
	};

	class FileStorage : public Storage
	{
	public:
		FileStorage(AssetManager *manager, std::string base) : base(base), Storage(manager) {}

		bool InputStream(const std::string &path, std::ifstream &stream, size_t *size = nullptr);
		bool OutputStream(const std::string &path, std::ofstream &stream);

		shared_ptr<Asset> Load(const std::string &path);

	private:
		std::string base;
	};

	class TarStorage : public Storage
	{
	public:
		TarStorage(shared_ptr<Asset> &tarFile) : tarFile(tarFile), Storage(tarFile->manager)
		{
			ReloadTarIndex();
		}

		bool InputStream(const std::string &path, std::ifstream &stream, size_t *size = nullptr);
		bool OutputStream(const std::string &path, std::ofstream &stream);

		shared_ptr<Asset> Load(const std::string &path);

	private:
		void ReloadTarIndex();

	private:
		shared_ptr<Asset> tarFile;
		OffsetLengthIndex tarIndex;
	};

	class VpkStorage : public Storage
	{
	public:
		VpkStorage(shared_ptr<Asset> &vpkFile) : vpkFile(vpkFile), Storage(vpkFile->manager)
		{
			ReloadVpkIndex();
		}

		bool InputStream(const std::string &path, std::ifstream &stream, size_t *size = nullptr);
		bool OutputStream(const std::string &path, std::ofstream &stream);

		shared_ptr<Asset> Load(const std::string &path);

	private:
		void ReloadVpkIndex();

	private:
		shared_ptr<Asset> vpkFile;
		OffsetLengthIndex vpkIndex;
	};
}
