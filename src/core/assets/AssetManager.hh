#pragma once

#include "core/Common.hh"
#include "ecs/Ecs.hh"

#include <map>
#include <picojson/picojson.h>
#include <string>
#include <tiny_gltf.h>
#include <unordered_map>

namespace sp {
    class Asset;
    class Model;
    class Scene;
    class Script;
    class Image;

    typedef std::unordered_map<std::string, weak_ptr<Asset>> AssetMap;
    typedef std::unordered_map<std::string, weak_ptr<Model>> ModelMap;
    typedef std::unordered_map<std::string, std::pair<size_t, size_t>> TarIndex;

    class AssetManager {
    public:
        AssetManager();

        bool InputStream(const std::string &path, std::ifstream &stream, size_t *size = nullptr);
        bool OutputStream(const std::string &path, std::ofstream &stream);

        // TinyGLTF filesystem functions
        bool ReadWholeFile(std::vector<unsigned char> *out, std::string *err, const std::string &path, void *user_data);
        bool FileExists(const std::string &abs_filename, void *user_data);
        std::string ExpandFilePath(const std::string &filepath, void *user_data);

        shared_ptr<const Asset> Load(const std::string &path);
        shared_ptr<Image> LoadImageByPath(const std::string &path);
        shared_ptr<Model> LoadModel(const std::string &name);
        shared_ptr<Scene> LoadScene(const std::string &name, ecs::Lock<ecs::AddRemove> lock, ecs::Owner owner);
        shared_ptr<Script> LoadScript(const std::string &path);

    private:
        void UpdateTarIndex();

    private:
        std::string base;
        AssetMap loadedAssets;
        ModelMap loadedModels;
        TarIndex tarIndex;

        tinygltf::TinyGLTF gltfLoader;
        tinygltf::FsCallbacks fs;
    };

    extern AssetManager GAssets;
} // namespace sp
