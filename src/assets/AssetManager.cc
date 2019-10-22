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
#include "ecs/components/Barrier.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/LightSensor.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/Mirror.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/TriggerArea.hh"
#include "ecs/components/View.hh"
#include "ecs/components/VoxelInfo.hh"
#include "ecs/components/LightGun.hh"
#include "ecs/components/SlideDoor.hh"
#include "ecs/components/Animation.hh"
#include "ecs/components/SignalReceiver.hh"

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
				else if (comp.first == "renderable")
				{
					auto r = entity.Assign<ecs::Renderable>();

					if (comp.second.is<string>())
					{
						r->model = LoadModel(comp.second.get<string>());
					}
					else
					{
						for (auto param : comp.second.get<picojson::object>())
						{
							if (param.first == "emissive")
							{
								r->emissive = MakeVec3(param.second);
							}
							else if (param.first == "light")
							{
								r->voxelEmissive = MakeVec3(param.second);
							}
							else if (param.first == "model")
							{
								r->model = LoadModel(param.second.get<string>());
							}
						}
					}
					Assert(!!r->model, "Renderable must have a model");

					if (glm::length(r->emissive) == 0.0f && glm::length(r->voxelEmissive) > 0.0f)
					{
						r->emissive = r->voxelEmissive;
					}
				}
				else if (comp.first == "bulb")
				{
					auto lightEnt = scene->FindEntity(comp.second.get<string>());
					auto light = lightEnt.Get<ecs::Light>();
					light->bulb = entity;
				}
				else if (comp.first == "lightsensor")
				{
					auto sensor = entity.Assign<ecs::LightSensor>();
					for (auto param : comp.second.get<picojson::object>())
					{
						if (param.first == "translate")
						{
							sensor->position = MakeVec3(param.second);
						}
						else if (param.first == "direction")
						{
							sensor->direction = MakeVec3(param.second);
						}
						else if (param.first == "outputTo")
						{
							for (auto entName : param.second.get<picojson::array>())
							{
								ecs::Entity receiverEnt =
									scene->FindEntity(entName.get<string>());
								if (!receiverEnt.Has<ecs::SignalReceiver>())
								{
									throw runtime_error(
										"\"outputTo\" entities must have a SignalReceiver");
								}
								auto receiver =
									receiverEnt.Get<ecs::SignalReceiver>();
								receiver->AttachSignal(entity);
							}
						}
						else if (param.first == "onColor")
						{
							sensor->onColor = MakeVec3(param.second);
						}
						else if (param.first == "offColor")
						{
							sensor->offColor = MakeVec3(param.second);
						}
						else if (param.first == "triggers")
						{
							for (auto trigger : param.second.get<picojson::array>())
							{
								ecs::LightSensor::Trigger tr;
								for (auto param : trigger.get<picojson::object>())
								{
									if (param.first == "illuminance")
									{
										tr.illuminance = MakeVec3(param.second);
									}
									else if (param.first == "oncmd")
									{
										tr.oncmd = param.second.get<string>();
									}
									else if (param.first == "offcmd")
									{
										tr.offcmd = param.second.get<string>();
									}
									else if (param.first == "onSignal")
									{
										tr.onSignal = param.second.get<double>();
									}
									else if (param.first == "offSignal")
									{
										tr.offSignal = param.second.get<double>();
									}
								}
								sensor->triggers.push_back(tr);
							}
						}
					}
				}
				else if (comp.first == "physics")
				{
					physx::PxRigidActor *actor = nullptr;

					shared_ptr<Model> model;
					PhysxActorDesc desc;

					for (auto param : comp.second.get<picojson::object>())
					{
						if (param.first == "model")
						{
							model = LoadModel(param.second.get<string>());
						}
						else if (param.first == "dynamic")
						{
							desc.dynamic = param.second.get<bool>();
						}
						else if (param.first == "kinematic")
						{
							desc.kinematic = param.second.get<bool>();
						}
						else if (param.first == "decomposeHull")
						{
							desc.decomposeHull = param.second.get<bool>();
						}
						else if (param.first == "density")
						{
							desc.density = param.second.get<double>();
						}
					}

					actor = px.CreateActor(model, desc, entity);

					if (actor)
					{
						auto physics = entity.Assign<ecs::Physics>(actor, model, desc);
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
