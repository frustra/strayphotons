#include "AssetManager.hh"

extern "C" {
#include <microtar.h>
}

#include "assets/Asset.hh"
#include "assets/AssetHelpers.hh"
#include "assets/Image.hh"
#include "assets/Model.hh"
#include "assets/Scene.hh"
#include "assets/Script.hh"
#include "core/Logging.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#ifdef LoadImage
    // I hate windows.h
    #undef LoadImage
#endif

namespace sp {
    AssetManager GAssets;

    const std::string ASSETS_DIR = "../assets/";
    const std::string ASSETS_TAR = "./assets.spdata";
    const std::string SHADERS_DIR = "../";

    AssetManager::AssetManager() {
        gltfLoaderCallbacks = std::make_unique<tinygltf::FsCallbacks>(tinygltf::FsCallbacks{
            // FileExists
            [](const std::string &abs_filename, void *userdata) -> bool {
                return true;
            },

            // ExpandFilePath
            [](const std::string &filepath, void *userdata) -> std::string {
                return filepath;
            },

            // ReadWholeFile
            [](std::vector<unsigned char> *out, std::string *err, const std::string &path, void *userdata) -> bool {
                std::ifstream in;
                size_t size;

                AssetManager *manager = static_cast<AssetManager *>(userdata);
                if (manager->InputStream(path, in, &size)) {
                    out->resize(size);
                    in.read((char *)out->data(), size);
                    in.close();
                    return true;
                }
                return false;
            },

            nullptr, // WriteFile callback, not supported
            this // Fs callback user data
        });

#ifdef SP_PACKAGE_RELEASE
        UpdateTarIndex();
#endif

        running = true;
        cleanupThread = std::thread([this] {
            while (this->running) {
                {
                    std::lock_guard lock(taskMutex);
                    for (auto it = runningTasks.begin(); it != runningTasks.end();) {
                        if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                            it = runningTasks.erase(it);
                        } else {
                            it++;
                        }
                    }
                }
                loadedModels.Tick();
                loadedAssets.Tick();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    AssetManager::~AssetManager() {
        running = false;
        cleanupThread.join();
    }

    void AssetManager::UpdateTarIndex() {
        mtar_t tar;
        if (mtar_open(&tar, ASSETS_TAR.c_str(), "r") != MTAR_ESUCCESS) {
            Errorf("Failed to open asset bundle at: %s", ASSETS_TAR);
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

    bool AssetManager::InputStream(const std::string &path, std::ifstream &stream, size_t *size) {
#ifdef SP_PACKAGE_RELEASE
        stream.open(ASSETS_TAR, std::ios::in | std::ios::binary);

        if (stream && tarIndex.count(path)) {
            auto indexData = tarIndex[path];
            if (size) *size = indexData.second;
            stream.seekg(indexData.first, std::ios::beg);
            return true;
        }

        return false;
#else
        std::string filename = (starts_with(path, "shaders/") ? SHADERS_DIR : ASSETS_DIR) + path;
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
        std::filesystem::path p(ASSETS_DIR + path);
        std::filesystem::create_directories(p.parent_path());

        stream.open(ASSETS_DIR + path, std::ios::out | std::ios::binary);
        return !!stream;
    }

    std::shared_ptr<const Asset> AssetManager::Load(const std::string &path) {
        Assert(!path.empty(), "AssetManager::Load called with empty path");

        auto asset = loadedAssets.Load(path);
        if (!asset) {
            {
                std::lock_guard lock(assetMutex);
                // Check again in case an inflight asset just completed on another thread
                asset = loadedAssets.Load(path);
                if (asset) return asset;

                asset = std::make_shared<Asset>(path);
                loadedAssets.Register(path, asset);
            }
            {
                std::lock_guard lock(taskMutex);
                runningTasks.emplace_back(std::async(std::launch::async, [this, path, asset] {
                    Debugf("Loading asset: %s", path);
                    std::ifstream in;
                    size_t size;

                    if (InputStream(path, in, &size)) {
                        asset->buffer.resize(size);
                        in.read((char *)asset->buffer.data(), size);
                        in.close();

                        asset->valid.test_and_set();
                        asset->valid.notify_all();
                    }
                }));
            }
        }

        return asset;
    }

    std::string findModel(const std::string &name) {
        std::string path;
#ifdef SP_PACKAGE_RELEASE
        path = "models/" + name + "/" + name + ".glb";
        if (tarIndex.count(path) > 0) return path;
        path = "models/" + name + ".glb";
        if (tarIndex.count(path) > 0) return path;
        path = "models/" + name + "/" + name + ".gltf";
        if (tarIndex.count(path) > 0) return path;
        path = "models/" + name + ".gltf";
        if (tarIndex.count(path) > 0) return path;
#else
        std::error_code ec;
        path = "models/" + name + "/" + name + ".glb";
        if (std::filesystem::is_regular_file(ASSETS_DIR + path, ec)) return path;
        path = "models/" + name + ".glb";
        if (std::filesystem::is_regular_file(ASSETS_DIR + path, ec)) return path;
        path = "models/" + name + "/" + name + ".gltf";
        if (std::filesystem::is_regular_file(ASSETS_DIR + path, ec)) return path;
        path = "models/" + name + ".gltf";
        if (std::filesystem::is_regular_file(ASSETS_DIR + path, ec)) return path;
#endif
        return "";
    }

    std::shared_ptr<const Model> AssetManager::LoadModel(const std::string &name) {
        Assert(!name.empty(), "AssetManager::LoadModel called with empty name");

        auto model = loadedModels.Load(name);
        if (!model) {
            {
                std::lock_guard lock(modelMutex);
                // Check again in case an inflight model just completed on another thread
                model = loadedModels.Load(name);
                if (model) return model;

                model = std::make_shared<Model>(name);
                loadedModels.Register(name, model);
            }
            {
                std::lock_guard lock(taskMutex);
                runningTasks.emplace_back(std::async(std::launch::async, [this, name, model] {
                    std::string path = findModel(name);
                    Assert(!path.empty(), "Model not found: " + name);

                    auto asset = Load(path);
                    model->PopulateFromAsset(asset, gltfLoaderCallbacks.get());
                }));
            }
        }

        return model;
    }

    std::shared_ptr<const Image> AssetManager::LoadImage(const std::string &path) {
        Assert(!path.empty(), "AssetManager::LoadImage called with empty path");

        auto image = loadedImages.Load(path);
        if (!image) {
            {
                std::lock_guard lock(imageMutex);
                // Check again in case an inflight image just completed on another thread
                image = loadedImages.Load(path);
                if (image) return image;

                image = std::make_shared<Image>();
                loadedImages.Register(path, image);
            }
            {
                std::lock_guard lock(taskMutex);
                runningTasks.emplace_back(std::async(std::launch::async, [this, path, image] {
                    auto asset = Load(path);
                    image->PopulateFromAsset(asset);
                }));
            }
        }

        return image;
    }

    shared_ptr<Scene> AssetManager::LoadScene(const std::string &name,
                                              ecs::Lock<ecs::AddRemove> lock,
                                              ecs::Owner owner) {
        Logf("Loading scene: %s", name);

        std::shared_ptr<const Asset> asset = Load("scenes/" + name + ".json");
        if (!asset) {
            Logf("Scene not found");
            return nullptr;
        }
        asset->WaitUntilValid();
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

        std::shared_ptr<const Asset> asset = Load("scripts/" + path);
        if (!asset) {
            Logf("Script not found");
            return nullptr;
        }
        asset->WaitUntilValid();

        std::stringstream ss(asset->String());
        vector<string> lines;

        string line;
        while (std::getline(ss, line, '\n')) {
            lines.emplace_back(std::move(line));
        }

        return make_shared<Script>(path, asset, std::move(lines));
    }
} // namespace sp
