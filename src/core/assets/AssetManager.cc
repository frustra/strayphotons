#include "AssetManager.hh"

extern "C" {
#include <microtar.h>
}

#include "assets/Asset.hh"
#include "assets/AssetHelpers.hh"
#include "assets/Gltf.hh"
#include "assets/Image.hh"
#include "assets/Script.hh"
#include "core/Tracing.hh"
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

    AssetManager::AssetManager() : RegisteredThread("AssetCleanup", 10.0), workQueue("AssetWorker", 4) {
#ifdef SP_PACKAGE_RELEASE
        UpdateTarIndex();
#endif
        StartThread();
    }

    void AssetManager::Frame() {
        loadedGltfs.Tick(this->interval);
        for (auto &assets : loadedAssets) {
            assets.Tick(this->interval);
        }
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

    bool AssetManager::InputStream(const std::string &path, AssetType type, std::ifstream &stream, size_t *size) {
        switch (type) {
        case AssetType::Bundled: {
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
        case AssetType::External: {
            stream.open(path, std::ios::in | std::ios::binary);

            if (size && stream) {
                stream.seekg(0, std::ios::end);
                *size = stream.tellg();
                stream.seekg(0, std::ios::beg);
            }

            return !!stream;
        }
        case AssetType::Count:
            break;
        }
        Abortf("AssetManager::InputStream called with invalid asset type: %s", path);
    }

    bool AssetManager::OutputStream(const std::string &path, std::ofstream &stream) {
        std::filesystem::path p(ASSETS_DIR + path);
        std::filesystem::create_directories(p.parent_path());

        stream.open(ASSETS_DIR + path, std::ios::out | std::ios::binary);
        return !!stream;
    }

    AsyncPtr<Asset> AssetManager::Load(const std::string &path, AssetType type, bool reload) {
        Assert(!path.empty(), "AssetManager::Load called with empty path");

        auto asset = loadedAssets[type].Load(path);
        if (!asset || reload) {
            std::lock_guard lock(assetMutex);
            if (!reload) {
                // Check again in case an inflight asset just completed on another thread
                asset = loadedAssets[type].Load(path);
                if (asset) return asset;
            }

            asset = workQueue.Dispatch<Asset>([this, path, type] {
                ZoneScopedN("LoadAsset");
                ZoneStr(path);
                std::ifstream in;
                size_t size;

                if (InputStream(path, type, in, &size)) {
                    auto asset = std::make_shared<Asset>(path);
                    asset->buffer.resize(size);
                    in.read((char *)asset->buffer.data(), size);
                    in.close();

                    return asset;
                } else {
                    Errorf("Asset does not exist: %s", path);
                    return std::shared_ptr<Asset>();
                }
            });
            loadedAssets[type].Register(path, asset, true /* allowReplace */);
        }

        return asset;
    }

    std::string findGltfByName(const std::string &name) {
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

    AsyncPtr<Gltf> AssetManager::LoadGltf(const std::string &name) {
        Assert(!name.empty(), "AssetManager::LoadGltf called with empty name");

        auto gltf = loadedGltfs.Load(name);
        if (!gltf) {
            {
                std::lock_guard lock(gltfMutex);
                // Check again in case an inflight model just completed on another thread
                gltf = loadedGltfs.Load(name);
                if (gltf) return gltf;

                std::string path;
                {
                    std::lock_guard lock2(externalGltfMutex);
                    auto it = externalGltfPaths.find(name);
                    if (it != externalGltfPaths.end()) path = it->second;
                }

                AsyncPtr<Asset> asset;
                if (path.empty()) {
                    path = findGltfByName(name);
                    if (!path.empty()) asset = Load(path, AssetType::Bundled);
                } else {
                    asset = Load(path, AssetType::External);
                }

                gltf = workQueue.Dispatch<Gltf>(asset, [this, name](std::shared_ptr<const Asset> asset) {
                    if (!asset) {
                        Logf("Gltf not found: %s", name);
                        return std::shared_ptr<Gltf>();
                    }
                    return std::make_shared<Gltf>(name, asset);
                });
                loadedGltfs.Register(name, gltf);
            }
        }

        return gltf;
    }

    AsyncPtr<Image> AssetManager::LoadImage(const std::string &path) {
        Assert(!path.empty(), "AssetManager::LoadImage called with empty path");

        auto image = loadedImages.Load(path);
        if (!image) {
            {
                std::lock_guard lock(imageMutex);
                // Check again in case an inflight image just completed on another thread
                image = loadedImages.Load(path);
                if (image) return image;

                auto asset = Load(path);
                image = workQueue.Dispatch<Image>(asset, [path](std::shared_ptr<const Asset> asset) {
                    if (!asset) {
                        Logf("Image not found: %s", path);
                        return std::shared_ptr<Image>();
                    }
                    return std::make_shared<Image>(asset);
                });

                loadedImages.Register(path, image);
            }
        }

        return image;
    }

    AsyncPtr<Script> AssetManager::LoadScript(const std::string &path) {
        Logf("Loading script: %s", path);

        auto asset = Load("scripts/" + path);
        return workQueue.Dispatch<Script>(asset, [path](std::shared_ptr<const Asset> asset) {
            if (!asset) {
                Logf("Script not found: %s", path);
                return std::shared_ptr<Script>();
            }

            return std::make_shared<Script>(path, asset);
        });
    }

    void AssetManager::RegisterExternalGltf(const std::string &name, const std::string &path) {
        std::lock_guard lock(externalGltfMutex);
        auto result = externalGltfPaths.emplace(name, path);
        if (result.second) {
            Assertf(result.first->second == path,
                "Duplicate gltf registration for: %s, %s != %s",
                name,
                result.second,
                path);
        }
    }

    bool AssetManager::IsGltfRegistered(const std::string &name) {
        std::lock_guard lock(externalGltfMutex);
        return externalGltfPaths.count(name) > 0;
    }
} // namespace sp
