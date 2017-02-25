#pragma once

#include "Common.hh"
#include <Ecs.hh>
#include "physx/PhysxManager.hh"
#include "graphics/Texture.hh"
#include <tiny_gltf_loader.h>

#include <unordered_map>
#include <string>
#include <map>

namespace sp
{
	class Asset;
	class Model;
	class Scene;

	typedef std::unordered_map<std::string, weak_ptr<Asset> > AssetMap;
	typedef std::unordered_map<std::string, weak_ptr<Model> > ModelMap;

	class AssetManager
	{
	public:
		bool InputStream(const std::string &path, std::ifstream &stream, size_t *size = nullptr);
		bool OutputStream(const std::string &path, std::ofstream &stream);

		shared_ptr<Asset> Load(const std::string &path);
		Texture LoadTexture(const std::string &path, GLsizei levels = Texture::FullyMipmap);
		shared_ptr<Model> LoadModel(const std::string &name);
		shared_ptr<Scene> LoadScene(const std::string &name, ecs::EntityManager *em, PhysxManager &px);

		void Unregister(const Asset &asset);
		void UnregisterModel(const Model &model);

	private:
		std::string base;
		AssetMap loadedAssets;
		ModelMap loadedModels;

		tinygltf::TinyGLTFLoader gltfLoader;
	};

	extern AssetManager GAssets;
}
