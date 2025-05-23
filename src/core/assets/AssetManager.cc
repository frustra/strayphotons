/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "AssetManager.hh"

extern "C" {
#include <microtar.h>
}

#include "assets/Asset.hh"
#include "assets/Gltf.hh"
#include "assets/Image.hh"
#include "assets/PhysicsInfo.hh"
#include "common/Tracing.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
// Hacky defines to prevent tinygltf from including windows.h and polluting the namespace
#ifdef _WIN32
    #define UNDEFINE_WIN32
    #undef _WIN32
    #define __ANDROID__
#endif
#include <tiny_gltf.h>
#ifdef UNDEFINE_WIN22
    #define _WIN32
    #undef __ANDROID__
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace sp {
    AssetManager &Assets() {
        static AssetManager assets;
        return assets;
    }

    const std::filesystem::path OVERRIDE_ASSETS_DIR = "./assets/";
    const std::filesystem::path DEFAULT_ASSETS_PATH = "./assets.spdata";

    AssetManager::AssetManager()
        : RegisteredThread("AssetCleanup", 10.0), shutdown(true), workQueue("AssetWorker", 4) {}

    AssetManager::~AssetManager() {
        Shutdown();
    }

    void AssetManager::StartThread(std::string assetsPath_) {
        bool wasShutdown = shutdown.exchange(false);
        if (!wasShutdown) return; // Thread already started
        if (!assetsPath_.empty()) {
            assetsPath = assetsPath_;
        } else {
            assetsPath = DEFAULT_ASSETS_PATH;
        }
        if (assetsPath.has_filename() && std::filesystem::is_regular_file(assetsPath)) {
            // Build bundle index
            mtar_t tar;
            std::string pathStr = assetsPath.string();
            if (mtar_open(&tar, pathStr.c_str(), "r") != MTAR_ESUCCESS) {
                Warnf("Failed to open asset bundle at: %s", pathStr);
                return;
            }

            mtar_header_t h;
            while (mtar_read_header(&tar, &h) != MTAR_ENULLRECORD) {
                size_t offset = tar.pos + 512 * sizeof(unsigned char);
                bundleIndex[h.name] = std::make_pair(offset, h.size);
                mtar_next(&tar);
            }

            mtar_close(&tar);
        }
        RegisteredThread::StartThread();
    }

    void AssetManager::Shutdown() {
        bool alreadyShutdown = shutdown.exchange(true);
        if (alreadyShutdown) return;
        StopThread(true);

        LogOnExit logOnExit = "Assets shut down ======================================================";
    }

    std::filesystem::path AssetManager::GetExternalPath(const std::string &path) const {
        if (std::filesystem::is_regular_file(OVERRIDE_ASSETS_DIR / path)) {
            return OVERRIDE_ASSETS_DIR / path;
        } else if (std::filesystem::is_regular_file(assetsPath / path)) {
            return assetsPath / path;
        } else {
            return std::filesystem::absolute(path);
        }
    }

    std::vector<std::string> AssetManager::ListBundledAssets(const std::string &prefix,
        const std::string &extension) const {
        std::vector<std::string> results;
        if (std::filesystem::is_directory(OVERRIDE_ASSETS_DIR / prefix)) {
            for (auto &entry : std::filesystem::directory_iterator(OVERRIDE_ASSETS_DIR / prefix)) {
                if (entry.is_regular_file() && (extension.empty() || entry.path().extension() == extension)) {
                    auto path = entry.path().lexically_relative(OVERRIDE_ASSETS_DIR).string();
                    std::replace(path.begin(), path.end(), (char)std::filesystem::path::preferred_separator, '/');
                    results.emplace_back(path);
                }
            }
        }
        if (std::filesystem::is_directory(assetsPath / prefix)) {
            for (auto &entry : std::filesystem::directory_iterator(assetsPath / prefix)) {
                auto path = entry.path().lexically_relative(assetsPath);
                if (std::filesystem::is_regular_file(OVERRIDE_ASSETS_DIR / path)) continue;
                if (entry.is_regular_file() && (extension.empty() || entry.path().extension() == extension)) {
                    std::string pathStr = path.string();
                    std::replace(pathStr.begin(), pathStr.end(), (char)std::filesystem::path::preferred_separator, '/');
                    results.emplace_back(pathStr);
                }
            }
        }
        if (!bundleIndex.empty()) {
            for (auto &[path, range] : bundleIndex) {
                if (starts_with(path, prefix)) {
                    if (std::filesystem::is_regular_file(OVERRIDE_ASSETS_DIR / path)) continue;
                    if (std::filesystem::is_regular_file(assetsPath / path)) continue;
                    results.emplace_back(path);
                }
            }
        }
        return results;
    }

    void AssetManager::Frame() {
        loadedGltfs.Tick(this->interval);
        for (auto &assets : loadedAssets) {
            assets.Tick(this->interval);
        }
    }

    bool AssetManager::InputStream(const std::string &path, AssetType type, std::ifstream &stream, size_t *size) {
        switch (type) {
        case AssetType::Bundled: {
            // Allow modding the asset bundle by placing files in OVERRIDE_ASSETS_DIR
            if (std::filesystem::is_regular_file(OVERRIDE_ASSETS_DIR / path)) {
                stream.open(OVERRIDE_ASSETS_DIR / path, std::ios::in | std::ios::binary);

                if (stream) {
                    if (size) {
                        stream.seekg(0, std::ios::end);
                        *size = stream.tellg();
                        stream.seekg(0, std::ios::beg);
                    }
                    return true;
                }
            } else if (!bundleIndex.empty()) {
                stream.open(assetsPath, std::ios::in | std::ios::binary);

                if (stream && bundleIndex.count(path)) {
                    auto indexData = bundleIndex[path];
                    if (size) *size = indexData.second;
                    stream.seekg(indexData.first, std::ios::beg);
                    return true;
                }
            } else if (std::filesystem::is_regular_file(assetsPath / path)) {
                stream.open(assetsPath / path, std::ios::in | std::ios::binary);

                if (stream) {
                    if (size) {
                        stream.seekg(0, std::ios::end);
                        *size = stream.tellg();
                        stream.seekg(0, std::ios::beg);
                    }
                    return true;
                }
            }

            return false;
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
        default:
            Abortf("AssetManager::InputStream called with invalid asset type: %s", path);
        }
    }

    bool AssetManager::OutputStream(const std::filesystem::path &path, std::ofstream &stream) {
        try {
            std::filesystem::create_directories(path.parent_path());
        } catch (std::filesystem::filesystem_error &err) {
            Errorf("Failed to create parent directory: %s", err.what());
            return false;
        }

        stream.open(path, std::ios::out | std::ios::binary);
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
                    Assertf(in.good(), "Failed to read whole asset file: %s", path);
                    in.close();

                    return asset;
                } else {
                    Warnf("Asset does not exist: %s", path);
                    return std::shared_ptr<Asset>();
                }
            });
            loadedAssets[type].Register(path, asset, true /* allowReplace */);
            if (shutdown.load()) StartThread();
        }

        return asset;
    }

    std::string AssetManager::FindGltfByName(const std::string &name) {
        const std::string pathOptions[] = {
            "models/" + name + "/" + name + ".glb",
            "models/" + name + ".glb",
            "models/" + name + "/" + name + ".gltf",
            "models/" + name + ".gltf",
        };
        for (const std::string &path : pathOptions) {
            if (std::filesystem::is_regular_file(OVERRIDE_ASSETS_DIR / path)) return path;
            if (bundleIndex.count(path)) return path;
            if (std::filesystem::is_regular_file(assetsPath / path)) return path;
        }
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
                    path = FindGltfByName(name);
                    if (!path.empty()) asset = Load(path, AssetType::Bundled);
                } else {
                    asset = Load(path, AssetType::External);
                }

                gltf = workQueue.Dispatch<Gltf>(asset, [name](std::shared_ptr<const Asset> asset) {
                    if (!asset) {
                        Logf("Gltf not found: %s", name);
                        return std::shared_ptr<Gltf>();
                    }
                    return std::make_shared<Gltf>(name, asset);
                });
                loadedGltfs.Register(name, gltf);
                if (shutdown.load()) StartThread();
            }
        }

        return gltf;
    }

    std::string AssetManager::FindPhysicsByName(const std::string &name) {
        const std::string pathOptions[] = {
            "models/" + name + "/" + name + ".physics.json",
            "models/" + name + "/physics.json",
            "models/" + name + ".physics.json",
        };
        for (const std::string &path : pathOptions) {
            if (std::filesystem::is_regular_file(OVERRIDE_ASSETS_DIR / path)) return path;
            if (bundleIndex.count(path)) return path;
            if (std::filesystem::is_regular_file(assetsPath / path)) return path;
        }
        return "";
    }

    AsyncPtr<PhysicsInfo> AssetManager::LoadPhysicsInfo(const std::string &name) {
        Assert(!name.empty(), "AssetManager::LoadPhysicsInfo called with empty name");

        auto physicsInfo = loadedPhysics.Load(name);
        if (!physicsInfo) {
            {
                std::lock_guard lock(physicsInfoMutex);
                // Check again in case an inflight model just completed on another thread
                physicsInfo = loadedPhysics.Load(name);
                if (physicsInfo) return physicsInfo;

                AsyncPtr<Asset> asset;
                auto path = FindPhysicsByName(name);
                if (!path.empty()) asset = Load(path, AssetType::Bundled);

                physicsInfo = workQueue.Dispatch<PhysicsInfo>(asset, [name](std::shared_ptr<const Asset> asset) {
                    // PhysicsInfo handles missing asset internally
                    return std::make_shared<PhysicsInfo>(name, asset);
                });
                loadedPhysics.Register(name, physicsInfo);
            }
        }

        return physicsInfo;
    }

    AsyncPtr<HullSettings> AssetManager::LoadHullSettings(const std::string &modelName, const std::string &meshName) {
        Assert(!modelName.empty(), "AssetManager::LoadHullSettings called with empty model name");
        Assert(!meshName.empty(), "AssetManager::LoadHullSettings called with empty mesh name");

        auto physicsInfo = LoadPhysicsInfo(modelName);

        return workQueue.Dispatch<HullSettings>(physicsInfo,
            [modelName, meshName](std::shared_ptr<const PhysicsInfo> physicsInfo) {
                if (!physicsInfo) {
                    Logf("PhysicsInfo not found: %s", modelName);
                    return std::shared_ptr<HullSettings>();
                }
                return std::make_shared<HullSettings>(PhysicsInfo::GetHull(physicsInfo, meshName));
            });
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
