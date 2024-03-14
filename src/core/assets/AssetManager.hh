/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Async.hh"
#include "common/DispatchQueue.hh"
#include "common/EnumTypes.hh"
#include "common/PreservingMap.hh"
#include "common/RegisteredThread.hh"
#include "ecs/Ecs.hh"

#include <atomic>
#include <future>
#include <mutex>
#include <robin_hood.h>
#include <string>
#include <thread>
#include <vector>

namespace sp {
    class Asset;
    class Gltf;
    class Image;
    class PhysicsInfo;
    struct HullSettings;

    enum class AssetType {
        Bundled = 0,
        External,
    };

    class AssetManager final : public RegisteredThread {
    public:
        AssetManager();
        ~AssetManager();
        void Shutdown();

        AsyncPtr<Asset> Load(const std::string &path, AssetType type = AssetType::Bundled, bool reload = false);
        AsyncPtr<Gltf> LoadGltf(const std::string &name);
        AsyncPtr<PhysicsInfo> LoadPhysicsInfo(const std::string &name);
        AsyncPtr<HullSettings> LoadHullSettings(const std::string &modelName, const std::string &meshName);
        AsyncPtr<Image> LoadImage(const std::string &path);

        void RegisterExternalGltf(const std::string &name, const std::string &path);
        bool IsGltfRegistered(const std::string &name);

        bool InputStream(const std::string &path, AssetType type, std::ifstream &stream, size_t *size = nullptr);
        bool OutputStream(const std::string &path, std::ofstream &stream);

    private:
        void Frame() override;

        void UpdateTarIndex();
        std::string FindGltfByName(const std::string &name);
        std::string FindPhysicsByName(const std::string &name);

        std::atomic_bool shutdown;
        DispatchQueue workQueue;

        std::mutex assetMutex;
        std::mutex gltfMutex;
        std::mutex physicsInfoMutex;
        std::mutex imageMutex;

        EnumArray<PreservingMap<std::string, Async<Asset>>, AssetType> loadedAssets;
        PreservingMap<std::string, Async<Gltf>> loadedGltfs;
        PreservingMap<std::string, Async<PhysicsInfo>> loadedPhysics;
        PreservingMap<std::string, Async<Image>> loadedImages;

        std::mutex externalGltfMutex;
        robin_hood::unordered_flat_map<std::string, std::string> externalGltfPaths;

        robin_hood::unordered_flat_map<std::string, std::pair<size_t, size_t>> tarIndex;
    };

    AssetManager &Assets();
} // namespace sp
