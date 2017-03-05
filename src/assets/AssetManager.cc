#define TINYGLTF_LOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "tiny_gltf_loader.h"

#include "assets/AssetManager.hh"
#include "assets/Asset.hh"
#include "assets/AssetHelpers.hh"
#include "assets/Model.hh"
#include "assets/Scene.hh"

#include "core/Logging.hh"
#include "Common.hh"

#include <Ecs.hh>
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/LightSensor.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/VoxelInfo.hh"
#include "ecs/components/Mirror.hh"
#include "ecs/components/Barrier.hh"

#include <boost/filesystem.hpp>
#include <iostream>
#include <fstream>

namespace sp
{
	AssetManager GAssets;

	const std::string ASSETS_DIR = "../assets/";

	bool AssetManager::InputStream(const std::string &path, std::ifstream &stream, size_t *size)
	{
		stream.open(ASSETS_DIR + path, std::ios::in | std::ios::binary);

		if (size && stream)
		{
			stream.seekg(0, std::ios::end);
			*size = stream.tellg();
			stream.seekg(0, std::ios::beg);
		}

		return !!stream;
	}

	bool AssetManager::OutputStream(const std::string &path, std::ofstream &stream)
	{
		boost::filesystem::path p(ASSETS_DIR + path);
		boost::filesystem::create_directories(p.parent_path());

		stream.open(ASSETS_DIR + path, std::ios::out | std::ios::binary);
		return !!stream;
	}

	shared_ptr<Asset> AssetManager::Load(const std::string &path)
	{
		AssetMap::iterator it = loadedAssets.find(path);
		shared_ptr<Asset> asset;

		if (it == loadedAssets.end() || it->second.expired())
		{
			Logf("Loading asset: %s", path);

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
			Assert(asset != nullptr, "Model asset not found");
			auto scene = make_shared<tinygltf::Scene>();
			std::string err;

			bool ret = gltfLoader.LoadASCIIFromString(scene.get(), &err, asset->CharBuffer(), asset->Size(), ASSETS_DIR + "models/" + name);
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

		auto entityList = root.get<picojson::object>()["entities"];
		for (auto value : entityList.get<picojson::array>())
		{
			ecs::Entity entity = em->NewEntity();
			auto ent = value.get<picojson::object>();
			for (auto comp : ent)
			{
				if (comp.first[0] == '_') continue;

				if (comp.first == "renderable")
				{
					auto model = LoadModel(comp.second.get<string>());
					entity.Assign<ecs::Renderable>(model);
				}
				else if (comp.first == "transform")
				{
					auto transform = entity.Assign<ecs::Transform>();
					for (auto subTransform : comp.second.get<picojson::object>())
					{
						if (subTransform.first == "relativeTo")
						{
							ecs::Entity parent = scene->FindEntity(subTransform.second.get<string>());
							if (!parent.Valid())
							{
								throw std::runtime_error("Entity relative to non-existent parent");
							}
							transform->SetRelativeTo(parent);
						}
						else
						{
							if (subTransform.first == "scale")
							{
								transform->Scale(MakeVec3(subTransform.second));
							}
							else if (subTransform.first == "rotate")
							{
								auto n = MakeVec4(subTransform.second);
								transform->Rotate(glm::radians(n[0]), { n[1], n[2], n[3] });
							}
							else if (subTransform.first == "translate")
							{
								transform->Translate(MakeVec3(subTransform.second));
							}
						}
					}
				}
				else if (comp.first == "view")
				{
					auto view = entity.Assign<ecs::View>();
					for (auto param : comp.second.get<picojson::object>())
					{
						if (param.first == "fov")
						{
							view->fov = glm::radians(param.second.get<double>());
						}
						else
						{
							if (param.first == "extents")
							{
								view->extents = MakeVec2(param.second);
							}
							else if (param.first == "clip")
							{
								view->clip = MakeVec2(param.second);
							}
							else if (param.first == "offset")
							{
								view->offset = MakeVec2(param.second);
							}
						}
					}
				}
				else if (comp.first == "light")
				{
					auto light = entity.Assign<ecs::Light>();
					for (auto param : comp.second.get<picojson::object>())
					{
						if (param.first == "intensity")
						{
							light->intensity = param.second.get<double>();
						}
						else if (param.first == "illuminance")
						{
							light->illuminance = param.second.get<double>();
						}
						else if (param.first == "spotAngle")
						{
							light->spotAngle = glm::radians(param.second.get<double>());
						}
						else if (param.first == "tint")
						{
							light->tint = MakeVec3(param.second);
						}
					}
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
					}
				}
				else if (comp.first == "physics")
				{
					physx::PxRigidActor *actor = nullptr;

					shared_ptr<Model> model;
					auto translate = physx::PxVec3 (0, 0, 0);
					auto scale = physx::PxVec3 (1, 1, 1);
					bool dynamic = true;
					bool kinematic = false;

					//auto rotate = physx::PxQuat (0);
					for (auto param : comp.second.get<picojson::object>())
					{
						if (param.first == "model")
						{
							model = LoadModel(param.second.get<string>());
						}
						if (param.first == "pxTranslate")
						{
							glm::vec3 gVec = MakeVec3(param.second);
							translate = physx::PxVec3(gVec.x, gVec.y, gVec.z);
						}
						else if (param.first == "pxScale")
						{
							glm::vec3 gVec = MakeVec3(param.second);
							scale = physx::PxVec3(gVec.x, gVec.y, gVec.z);
						}
						else if (param.first == "pxRotate")
						{
							//rotate = MakeVec3(param.second);
						}
						else if (param.first == "dynamic")
						{
							dynamic = param.second.get<bool>();
						}
						else if (param.first == "kinematic")
						{
							kinematic = param.second.get<bool>();
						}
					}

					PhysxManager::ActorDesc desc;
					desc.transform = physx::PxTransform(translate);
					desc.scale = physx::PxMeshScale(scale, physx::PxQuat(physx::PxIdentity));
					desc.dynamic = dynamic;
					desc.kinematic = kinematic;

					actor = px.CreateActor(model, desc);

					if (actor)
					{
						auto physics = entity.Assign<ecs::Physics>(actor, model);
					}
				}
				else if (comp.first == "barrier")
				{
					auto barrier = entity.Assign<ecs::Barrier>();

					for (auto param : comp.second.get<picojson::object>())
					{
						if (param.first == "isOpen")
						{
							barrier->isOpen = param.second.get<bool>();
						}
					}

					if (barrier->isOpen)
					{
						if (!entity.Has<ecs::Physics>() || !entity.Has<ecs::Renderable>())
						{
							throw std::runtime_error(
								"barrier component must come after Physics and Renderable"
							);
						}
						ecs::Barrier::Open(entity, px);
					}
				}
				else if (comp.first == "barrier_prefab")
				{
					glm::vec3 translate;
					glm::vec3 scale;
					bool isOpen = false;

					vector<string> reqParams = {"translate", "scale"};

					ParameterCheck(comp, reqParams);
					for (auto param : comp.second.get<picojson::object>())
					{
						if (param.first == "isOpen")
						{
							isOpen = param.second.get<bool>();
						}
						else if (param.first == "translate")
						{
							translate = MakeVec3(param.second);
						}
						else if (param.first == "scale")
						{
							scale = MakeVec3(param.second);
						}
					}

					ecs::Barrier::Create(translate, scale, px, *em);
					if (isOpen)
					{
						ecs::Barrier::Open(entity, px);
					}
				}
				else if (comp.first == "voxels")
				{
					auto voxelInfo = entity.Assign<ecs::VoxelInfo>();
					for (auto param : comp.second.get<picojson::object>())
					{
						if (param.first == "min")
						{
							voxelInfo->gridMin = MakeVec3(param.second) - glm::vec3(0.1);
						}
						else if (param.first == "max")
						{
							voxelInfo->gridMax = MakeVec3(param.second) + glm::vec3(0.1);
						}
					}
				}
				else if (comp.first == "mirror")
				{
					auto mirror = entity.Assign<ecs::Mirror>();
					mirror->size = MakeVec2(comp.second);
				}
			}
			if (ent.count("_name"))
			{
				scene->namedEntities[ent["_name"].get<string>()] = entity;
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

	void AssetManager::ParameterCheck(
		std::pair<const string, picojson::value> &jsonComp,
		vector<string> reqParams)
	{
		vector<bool> found(reqParams.size(), false);

		for (auto param : jsonComp.second.get<picojson::object>())
		{
			for (uint i = 0; i < found.size(); ++i)
			{
				if (param.first == reqParams.at(i))
				{
					found.at(i) = true;
					break;
				}
			}
		}

		for (uint i = 0; i < found.size(); ++i)
		{
			if (!found.at(i))
			{
				std::stringstream ss;
				ss << "\"" << jsonComp.first << "\""
				   << " gltf component is missing required \""
				   << reqParams.at(i) << "\" field";
				throw std::runtime_error(ss.str());
			}
		}
	}
}
