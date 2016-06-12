#define TINYGLTF_LOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "tiny_gltf_loader.h"

#include "assets/AssetManager.hh"
#include "assets/Asset.hh"
#include "assets/Model.hh"
#include "assets/Scene.hh"

#include "core/Logging.hh"

#include "ecs/Ecs.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/View.hh"

#include <iostream>
#include <fstream>

namespace sp
{
	const std::string ASSETS_DIR = "../assets/";

	shared_ptr<Asset> AssetManager::Load(const std::string &path)
	{
		Debugf("Loading asset: %s", path.c_str());
		AssetMap::iterator it = loadedAssets.find(path);
		shared_ptr<Asset> asset;

		if (it == loadedAssets.end())
		{
			std::ifstream in(ASSETS_DIR + path, std::ios::in | std::ios::binary);

			if (in)
			{
				in.seekg(0, std::ios::end);
				size_t size = in.tellg();
				char *buffer = new char[size];
				in.seekg(0, std::ios::beg);
				in.read(buffer, size);
				in.close();

				asset = make_shared<Asset>(this, path, buffer, size);
				loadedAssets[path] = weak_ptr<Asset>(asset);
			}
			else
			{
				throw "Invalid asset path";
			}
		}
		else
		{
			asset = it->second.lock();
		}

		return asset;
	}

	shared_ptr<Model> AssetManager::LoadModel(const std::string &name)
	{
		Debugf("Loading model: %s", name.c_str());
		ModelMap::iterator it = loadedModels.find(name);
		shared_ptr<Model> model;

		if (it == loadedModels.end())
		{
			shared_ptr<Asset> asset = Load("models/" + name + "/" + name + ".gltf");
			tinygltf::Scene *scene = new tinygltf::Scene();
			std::string err;

			bool ret = gltfLoader.LoadASCIIFromString(scene, &err, asset->Buffer(), asset->Size(), ASSETS_DIR + "models/" + name);
			if (!err.empty())
			{
				throw err.c_str();
			}
			else if (!ret)
			{
				throw "Failed to parse glTF";
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

	shared_ptr<Scene> AssetManager::LoadScene(const std::string &name, EntityManager *em)
	{
		Debugf("Loading scene: %s", name.c_str());

		shared_ptr<Asset> asset = Load("scenes/" + name + ".json");
		picojson::value root;
		std::string err = picojson::parse(root, asset->Buffer());
		if (!err.empty())
		{
			Errorf(err);
			return NULL;
		}

		shared_ptr<Scene> scene = make_shared<Scene>(name, asset);
		vector<double> numbers;

		auto entityList = root.get<picojson::object>()["entities"];
		for (auto value : entityList.get<picojson::array>())
		{
			Entity entity = em->NewEntity();
			auto ent = value.get<picojson::object>();
			for (auto comp : ent)
			{
				if (comp.first[0] == '_') continue;

				if (comp.first == "renderable")
				{
					auto model = LoadModel(comp.second.get<string>());
					entity.Assign<ECS::Renderable>(model);
				}
				else if (comp.first == "transform")
				{
					ECS::Transform *transform = entity.Assign<ECS::Transform>();
					for (auto subTransform : comp.second.get<picojson::object>())
					{
						if (subTransform.first == "relativeTo")
						{
							transform->SetRelativeTo(scene->FindEntity(subTransform.second.get<string>()));
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
					ECS::View *view = entity.Assign<ECS::View>();
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
}

