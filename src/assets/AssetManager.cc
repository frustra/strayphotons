#define TINYGLTF_LOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "tiny_gltf_loader.h"

#include "assets/AssetManager.hh"
#include "assets/Asset.hh"
#include "assets/Model.hh"
#include "assets/Scene.hh"

#include "core/Logging.hh"

#include <Ecs.hh>
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/VoxelInfo.hh"

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

		if (it == loadedAssets.end())
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
				throw std::runtime_error("Invalid asset path");
			}
		}
		else
		{
			asset = it->second.lock();
		}

		return asset;
	}

	Texture AssetManager::LoadTexture(const std::string &path)
	{
		return Texture().Create().LoadFromAsset(Load(path));
	}

	shared_ptr<Model> AssetManager::LoadModel(const std::string &name)
	{
		ModelMap::iterator it = loadedModels.find(name);
		shared_ptr<Model> model;

		if (it == loadedModels.end())
		{
			Logf("Loading model: %s", name);
			shared_ptr<Asset> asset = Load("models/" + name + "/" + name + ".gltf");
			tinygltf::Scene *scene = new tinygltf::Scene();
			std::string err;

			bool ret = gltfLoader.LoadASCIIFromString(scene, &err, asset->CharBuffer(), asset->Size(), ASSETS_DIR + "models/" + name);
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
		picojson::value root;
		string err = picojson::parse(root, asset->String());
		if (!err.empty())
		{
			Errorf(err);
			return nullptr;
		}

		shared_ptr<Scene> scene = make_shared<Scene>(name, asset);
		vector<double> numbers;

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
							auto values = subTransform.second.get<picojson::array>();
							numbers.resize(values.size());
							for (size_t i = 0; i < values.size(); i++)
							{
								numbers[i] = values[i].get<double>();
							}

							if (subTransform.first == "scale")
							{
								transform->Scale(glm::make_vec3(&numbers[0]));
							}
							else if (subTransform.first == "rotate")
							{
								transform->Rotate(glm::radians(numbers[0]), glm::make_vec3(&numbers[1]));
							}
							else if (subTransform.first == "translate")
							{
								transform->Translate(glm::make_vec3(&numbers[0]));
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
							auto values = param.second.get<picojson::array>();
							numbers.resize(values.size());
							for (size_t i = 0; i < values.size(); i++)
							{
								numbers[i] = values[i].get<double>();
							}

							if (param.first == "extents")
							{
								view->extents = glm::make_vec2(&numbers[0]);
							}
							else if (param.first == "clip")
							{
								view->clip = glm::make_vec2(&numbers[0]);
							}
							else if (param.first == "offset")
							{
								view->offset = glm::make_vec2(&numbers[0]);
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
							auto values = param.second.get<picojson::array>();
							numbers.resize(values.size());
							for (size_t i = 0; i < values.size(); i++)
							{
								numbers[i] = values[i].get<double>();
							}

							light->tint = glm::make_vec3(&numbers[0]);
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

					//auto rotate = physx::PxQuat (0);
					for (auto param : comp.second.get<picojson::object>())
					{
						if (param.first == "model")
						{
							model = LoadModel(param.second.get<string>());
						}
						if (param.first == "pxTranslate")
						{
							auto values = param.second.get<picojson::array>();
							numbers.resize(values.size());

							for (size_t i = 0; i < values.size(); i++)
							{
								numbers[i] = values[i].get<double>();
							}

							glm::vec3 gVec = glm::make_vec3(&numbers[0]);
							translate = physx::PxVec3 (physx::PxReal(gVec.x), physx::PxReal(gVec.y), physx::PxReal(gVec.z));
						}
						else if (param.first == "pxScale")
						{
							auto values = param.second.get<picojson::array>();
							numbers.resize(values.size());

							for (size_t i = 0; i < values.size(); i++)
							{
								numbers[i] = values[i].get<double>();
							}

							glm::vec3 gVec = glm::make_vec3(&numbers[0]);
							scale = physx::PxVec3 (physx::PxReal(gVec.x), physx::PxReal(gVec.y), physx::PxReal(gVec.z));
						}
						else if (param.first == "pxRotate")
						{
							//rotate = glm::make_vec3(&numbers[0]);
						}
						else if (param.first == "dynamic")
						{
							dynamic = param.second.get<bool>();
						}
					}

					PhysxManager::ActorDesc desc;
					desc.transform = physx::PxTransform(translate);
					desc.scale = physx::PxMeshScale(scale, physx::PxQuat(physx::PxIdentity));
					desc.dynamic = dynamic;

					actor = px.CreateActor(model, desc);

					if (actor)
					{
						auto physics = entity.Assign<ecs::Physics>(actor);
					}
				}
				else if (comp.first == "voxels")
				{
					auto voxelInfo = entity.Assign<ecs::VoxelInfo>();
					for (auto param : comp.second.get<picojson::object>())
					{
						if (param.first == "size")
						{
							voxelInfo->voxelSize = param.second.get<double>();
						}
						else if (param.first == "center")
						{
							auto values = param.second.get<picojson::array>();
							numbers.resize(values.size());
							for (size_t i = 0; i < values.size(); i++)
							{
								numbers[i] = values[i].get<double>();
							}

							voxelInfo->voxelGridCenter = glm::make_vec3(&numbers[0]);
						}
					}
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
}
