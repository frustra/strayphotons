#pragma once

#include "Common.hh"
#include <Ecs.hh>
#include "physx/PhysxManager.hh"
#include "graphics/Texture.hh"
#include <tiny_gltf_loader.h>
#include <tinygltfloader/picojson.h>

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
	typedef std::unordered_map<std::string, std::pair<size_t, size_t> > TarIndex;

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

		/**
		 * throws an std::runtime_error if a required parameter is not found
		 */
		void ParameterCheck(
			std::pair<const string, picojson::value> &jsonComp,
			vector<string> reqParams);

		void UpdateTarIndex();

	private:
		std::string base;
		AssetMap loadedAssets;
		ModelMap loadedModels;
		TarIndex tarIndex;

		tinygltf::TinyGLTFLoader gltfLoader;
	};

	extern AssetManager GAssets;
}
