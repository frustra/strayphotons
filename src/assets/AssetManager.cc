#define TINYGLTF_LOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <tiny_gltf_loader.h>

extern "C"
{
#include <microtar.h>
}

#include "assets/AssetManager.hh"
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

namespace sp
{
	AssetManager GAssets;

	const std::string ASSETS_DIR = "../assets/";
	const std::string ASSETS_TAR = "./assets.spdata";
	const std::string SHADERS_DIR = "../src/";

	AssetManager::AssetManager()
	{
		tinygltf::OpenFileCallback = [](const std::string & path, std::ifstream & stream, size_t *size) -> bool
		{
			return GAssets.InputStream(path, stream, size);
		};
	}

	void AssetManager::UpdateTarIndex()
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

	bool AssetManager::InputStream(const std::string &path, std::ifstream &stream, size_t *size)
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
		string filename = (starts_with(path, "shaders/") ? SHADERS_DIR : ASSETS_DIR) + path;
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

	bool AssetManager::OutputStream(const std::string &path, std::ofstream &stream)
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

	Texture AssetManager::LoadTexture(const std::string &path, GLsizei levels)
	{
		auto asset = Load(path);
		Assert(asset != nullptr, "Texture asset not found");
		return Texture().Create().LoadFromAsset(asset, levels);
	}

	shared_ptr<Model> AssetManager::LoadModel(const std::string &name)
	{
		ModelMap::iterator it = loadedModels.find(name);
		shared_ptr<Model> model;

		if (it == loadedModels.end() || it->second.expired())
		{
			Logf("Loading model: %s", name);
			shared_ptr<Asset> asset = Load("models/" + name + "/" + name + ".gltf");
			if (!asset) asset = Load("models/" + name + ".gltf");
			Assert(asset != nullptr, "Model asset not found");
			auto scene = make_shared<tinygltf::Scene>();
			std::string err;

#ifdef PACKAGE_RELEASE
			bool ret = gltfLoader.LoadASCIIFromString(scene.get(), &err, asset->CharBuffer(), asset->Size(), "models/" + name);
#else
			bool ret = gltfLoader.LoadASCIIFromString(scene.get(), &err, asset->CharBuffer(), asset->Size(), ASSETS_DIR + "models/" + name);
#endif
			if (!err.empty())
			{
				throw std::runtime_error(err);
			}
			else if (!ret)
			{
				throw std::runtime_error("Failed to parse glTF");
			}

			model = make_shared<Model>(name, asset, scene);
			loadedModels[name] = weak_ptr<Model>(model);
		}
		else
		{
			model = it->second.lock();
		}

		return model;
	}

	shared_ptr<Scene> AssetManager::LoadScene(const std::string &name, ecs::EntityManager *em, PhysxManager &px)
	{
		Logf("Loading scene: %s", name);

		shared_ptr<Asset> asset = Load("scenes/" + name + ".json");
		if (!asset)
		{
			Logf("Scene not found");
			return nullptr;
		}
		picojson::value root;
		string err = picojson::parse(root, asset->String());
		if (!err.empty())
		{
			Errorf(err);
			return nullptr;
		}

		shared_ptr<Scene> scene = make_shared<Scene>(name, asset);

		auto autoExecList = root.get<picojson::object>()["autoexec"];
		if (autoExecList.is<picojson::array>())
		{
			for (auto value : autoExecList.get<picojson::array>())
			{
				auto line = value.get<string>();
				scene->autoExecList.push_back(line);
			}
		}

		auto unloadExecList = root.get<picojson::object>()["unloadexec"];
		if (unloadExecList.is<picojson::array>())
		{
			for (auto value : unloadExecList.get<picojson::array>())
			{
				auto line = value.get<string>();
				scene->unloadExecList.push_back(line);
			}
		}

		auto entityList = root.get<picojson::object>()["entities"];
		for (auto value : entityList.get<picojson::array>())
		{
			ecs::Entity entity = em->NewEntity();
			auto ent = value.get<picojson::object>();
			for (auto comp : ent)
			{
				if (comp.first[0] == '_') continue;

				auto componentType = ecs::LookupComponent(comp.first);
				if (componentType != nullptr)
				{
					bool result = componentType->LoadEntity(entity, comp.second);
					if (!result)
					{
						throw std::runtime_error("Failed to load component type: " + comp.first);
					}
				}
				else
				{
					Errorf("Unknown component, ignoring: %s", comp.first);
				}
			}
			if (ent.count("_name"))
			{
				auto name = ent["_name"].get<string>();
				entity.AssignKey<ecs::Name>(name);
				scene->namedEntities[name] = entity;
			}
			scene->entities.push_back(entity);
		}
		return scene;
	}

	void AssetManager::Unregister(const Asset &asset)
	{
		loadedAssets.erase(asset.path);
	}

	void AssetManager::UnregisterModel(const Model &model)
	{
		loadedModels.erase(model.name);
	}
}
