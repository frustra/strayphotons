extern "C" {
#include <microtar.h>
}

#include "assets/Asset.hh"
#include "assets/AssetHelpers.hh"
#include "assets/AssetManager.hh"
#include "assets/Image.hh"
#include "assets/Model.hh"
#include "assets/Scene.hh"
#include "assets/Script.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"

#if !(__APPLE__)
    #include <filesystem>
#endif
#include <fstream>
#include <iostream>
#include <utility>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace sp {
    AssetManager GAssets;

    const std::string ASSETS_DIR = "../assets/";
    const std::string ASSETS_TAR = "./assets.spdata";
    const std::string SHADERS_DIR = "../";

    AssetManager::AssetManager() {
        fs = {
            [](const std::string &abs_filename, void *user_data) -> bool {
                return GAssets.FileExists(abs_filename, user_data);
            },

            [](const std::string &filepath, void *user_data) -> std::string {
                return GAssets.ExpandFilePath(filepath, user_data);
            },

            [](std::vector<unsigned char> *out, std::string *err, const std::string &path, void *user_data) -> bool {
                return GAssets.ReadWholeFile(out, err, path, user_data);
            },

            nullptr, // WriteFile callback, not supported
            nullptr // Fs callback user data
        };

        gltfLoader.SetFsCallbacks(fs);
    }

    void AssetManager::UpdateTarIndex() {
        mtar_t tar;
        if (mtar_open(&tar, ASSETS_TAR.c_str(), "r") != MTAR_ESUCCESS) {
            Errorf("Failed to build asset index");
            return;
        }

        mtar_header_t h;
        while (mtar_read_header(&tar, &h) != MTAR_ENULLRECORD) {
            size_t offset = tar.pos + 512 * sizeof(unsigned char);
            tarIndex[h.name] = std::make_pair(offset, h.size);
            mtar_next(&tar);
        }

        mtar_close(&tar);
    }

    std::string AssetManager::ExpandFilePath(const std::string &filepath, void * /* user_data */) {
        return filepath;
    }

    bool AssetManager::FileExists(const std::string &abs_filename, void * /* user_data */) {
        return true;
    }

    bool AssetManager::ReadWholeFile(std::vector<unsigned char> *out,
                                     std::string *err,
                                     const std::string &path,
                                     void * /* user_data */) {
        std::ifstream stream;

#ifdef PACKAGE_RELEASE
        if (tarIndex.size() == 0) UpdateTarIndex();

        stream.open(ASSETS_TAR, std::ios::in | std::ios::binary);

        if (stream && tarIndex.count(path)) {
            // Get the start and end location of the requested file in the tarIndex
            auto indexData = tarIndex[path];

            // Move the stream head to the start location of the file in the tarIndex
            stream.seekg(indexData.first, std::ios::beg);

            // Resize the output vector to match the size of the file to be read
            out->resize(indexData.second, '\0');

            // Copy bytes from the file into the output buffer
            stream.read(reinterpret_cast<char *>(&out->at(0)), static_cast<std::streamsize>(indexData.second));
            stream.close();
            return true;
        }
#else
        stream.open(path, std::ios::in | std::ios::binary);

        if (stream) {
            // Move the head to the end of the file
            stream.seekg(0, std::ios::end);

            // Get the head position (since we're at the end, this is the filesize)
            size_t fileSize = stream.tellg();

            // Move back to the beginning
            stream.seekg(0, std::ios::beg);

            // Make output buffer fit the file
            out->resize(fileSize, '\0');

            // Copy bytes from the file into the output buffer
            stream.read(reinterpret_cast<char *>(&out->at(0)), static_cast<std::streamsize>(fileSize));
            stream.close();

            return true;
        }
#endif
        return false;
    }

    bool AssetManager::InputStream(const std::string &path, std::ifstream &stream, size_t *size) {
#ifdef PACKAGE_RELEASE
        if (tarIndex.size() == 0) UpdateTarIndex();

        stream.open(ASSETS_TAR, std::ios::in | std::ios::binary);

        if (stream && tarIndex.count(path)) {
            auto indexData = tarIndex[path];
            if (size) *size = indexData.second;
            stream.seekg(indexData.first, std::ios::beg);
            return true;
        }

        return false;
#else
        string filename = (starts_with(path, "shaders/") ? SHADERS_DIR : ASSETS_DIR) + path;
        stream.open(filename, std::ios::in | std::ios::binary);

        if (size && stream) {
            stream.seekg(0, std::ios::end);
            *size = stream.tellg();
            stream.seekg(0, std::ios::beg);
        }

        return !!stream;
#endif
    }

    bool AssetManager::OutputStream(const std::string &path, std::ofstream &stream) {
#if !(__APPLE__)
        std::filesystem::path p(ASSETS_DIR + path);
        std::filesystem::create_directories(p.parent_path());
#endif

        stream.open(ASSETS_DIR + path, std::ios::out | std::ios::binary);
        return !!stream;
    }

    shared_ptr<Asset> AssetManager::Load(const std::string &path) {
        AssetMap::iterator it = loadedAssets.find(path);
        shared_ptr<Asset> asset;

        if (it != loadedAssets.end()) { asset = it->second.lock(); }

        if (!asset) {
            std::ifstream in;
            size_t size;

            if (InputStream(path, in, &size)) {
                uint8 *buffer = new uint8[size];
                in.read((char *)buffer, size);
                in.close();

                asset = make_shared<Asset>(this, path, buffer, size);
                loadedAssets[path] = weak_ptr<Asset>(asset);
            } else {
                return nullptr;
            }
        }

        return asset;
    }

    shared_ptr<Image> AssetManager::LoadImageByPath(const std::string &path) {
        auto asset = Load(path);
        Assert(asset != nullptr, "Image asset not found");

        return make_shared<Image>(asset);
    }

    shared_ptr<Model> AssetManager::LoadModel(const std::string &name) {
        ModelMap::iterator it = loadedModels.find(name);
        shared_ptr<Model> model;

        if (it == loadedModels.end() || it->second.expired()) {
            Logf("Loading model: %s", name);

            auto gltfModel = make_shared<tinygltf::Model>();
            std::string err;
            std::string warn;
            bool ret = false;

            // Check if there is a .glb version of the model and prefer that
            shared_ptr<Asset> asset = Load("models/" + name + "/" + name + ".glb");
            if (!asset) asset = Load("models/" + name + ".glb");

            // Found a GLB
            if (asset) {
#ifdef PACKAGE_RELEASE
                ret = gltfLoader.LoadBinaryFromMemory(gltfModel.get(),
                                                      &err,
                                                      &warn,
                                                      (const unsigned char *)asset->CharBuffer(),
                                                      asset->Size(),
                                                      "models/" + name);
#else
                ret = gltfLoader.LoadBinaryFromMemory(gltfModel.get(),
                                                      &err,
                                                      &warn,
                                                      (const unsigned char *)asset->CharBuffer(),
                                                      asset->Size(),
                                                      ASSETS_DIR + "models/" + name);
#endif
            } else {
                asset = Load("models/" + name + "/" + name + ".gltf");
                if (!asset) asset = Load("models/" + name + ".gltf");

#ifdef PACKAGE_RELEASE
                ret = gltfLoader.LoadASCIIFromString(gltfModel.get(),
                                                     &err,
                                                     &warn,
                                                     asset->CharBuffer(),
                                                     asset->Size(),
                                                     "models/" + name);
#else
                ret = gltfLoader.LoadASCIIFromString(gltfModel.get(),
                                                     &err,
                                                     &warn,
                                                     asset->CharBuffer(),
                                                     asset->Size(),
                                                     ASSETS_DIR + "models/" + name);
#endif
            }

            Assert(asset != nullptr, "Model asset not found");

            if (!err.empty()) {
                throw std::runtime_error(err);
            } else if (!ret) {
                throw std::runtime_error("Failed to parse glTF");
            }

            model = make_shared<Model>(name, asset, gltfModel);
            loadedModels[name] = weak_ptr<Model>(model);
        } else {
            model = it->second.lock();
        }

        return model;
    }

    shared_ptr<Scene> AssetManager::LoadScene(const std::string &name,
                                              ecs::Lock<ecs::AddRemove> lock,
                                              ecs::Owner owner) {
        Logf("Loading scene: %s", name);

        shared_ptr<Asset> asset = Load("scenes/" + name + ".json");
        if (!asset) {
            Logf("Scene not found");
            return nullptr;
        }
        picojson::value root;
        string err = picojson::parse(root, asset->String());
        if (!err.empty()) {
            Errorf("%s", err);
            return nullptr;
        }

        shared_ptr<Scene> scene = make_shared<Scene>(name, asset);

        auto autoExecList = root.get<picojson::object>()["autoexec"];
        if (autoExecList.is<picojson::array>()) {
            for (auto value : autoExecList.get<picojson::array>()) {
                auto line = value.get<string>();
                scene->autoExecList.push_back(line);
            }
        }

        auto unloadExecList = root.get<picojson::object>()["unloadexec"];
        if (unloadExecList.is<picojson::array>()) {
            for (auto value : unloadExecList.get<picojson::array>()) {
                auto line = value.get<string>();
                scene->unloadExecList.push_back(line);
            }
        }

        std::unordered_map<std::string, Tecs::Entity> namedEntities;

        auto entityList = root.get<picojson::object>()["entities"];
        // Find all named entities first so they can be referenced.
        for (auto value : entityList.get<picojson::array>()) {
            auto ent = value.get<picojson::object>();

            if (ent.count("_name")) {
                Tecs::Entity entity = lock.NewEntity();
                auto name = ent["_name"].get<string>();
                entity.Set<ecs::Name>(lock, name);
                if (namedEntities.count(name) != 0) { throw std::runtime_error("Duplicate entity name: " + name); }
                namedEntities.emplace(name, entity);
            }
        }

        for (auto value : entityList.get<picojson::array>()) {
            auto ent = value.get<picojson::object>();

            Tecs::Entity entity;
            if (ent.count("_name")) {
                entity = namedEntities[ent["_name"].get<string>()];
            } else {
                entity = lock.NewEntity();
            }

            entity.Set<ecs::Owner>(lock, owner);
            for (auto comp : ent) {
                if (comp.first[0] == '_') continue;

                auto componentType = ecs::LookupComponent(comp.first);
                if (componentType != nullptr) {
                    bool result = componentType->LoadEntity(lock, entity, comp.second);
                    if (!result) { throw std::runtime_error("Failed to load component type: " + comp.first); }
                } else {
                    Errorf("Unknown component, ignoring: %s", comp.first);
                }
            }
            scene->entities.push_back(entity);
        }
        return scene;
    }

    shared_ptr<Script> AssetManager::LoadScript(const std::string &path) {
        Logf("Loading script: %s", path);

        shared_ptr<Asset> asset = Load("scripts/" + path);
        if (!asset) {
            Logf("Script not found");
            return nullptr;
        }

        std::stringstream ss(asset->String());
        vector<string> lines;

        string line;
        while (std::getline(ss, line, '\n')) {
            lines.emplace_back(std::move(line));
        }

        return make_shared<Script>(path, asset, std::move(lines));
    }

    void AssetManager::Unregister(const Asset &asset) {
        loadedAssets.erase(asset.path);
    }

    void AssetManager::UnregisterModel(const Model &model) {
        loadedModels.erase(model.name);
    }
} // namespace sp
