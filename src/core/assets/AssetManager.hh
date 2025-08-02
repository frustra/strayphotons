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
#include "common/Hashing.hh"
#include "common/PreservingMap.hh"
#include "common/RegisteredThread.hh"
#include "ecs/Ecs.hh"

#include <atomic>
#include <filesystem>
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

    extern const std::filesystem::path OVERRIDE_ASSETS_DIR;

    using AssetName = InlineString<255>;
    using AssetPath = InlineString<255>;

    enum class AssetType {
        Bundled = 0,
        External,
    };

    class AssetManager final : public RegisteredThread {
    public:
        AssetManager();
        ~AssetManager();

        void StartThread(std::string assetsPath = "");
        void Shutdown();

        std::filesystem::path GetExternalPath(std::string_view path) const;
        std::vector<std::string> ListBundledAssets(std::string_view prefix, std::string_view extension = "") const;

        AsyncPtr<Asset> Load(std::string_view path, AssetType type = AssetType::Bundled, bool reload = false);
        AsyncPtr<Gltf> LoadGltf(std::string_view name);
        AsyncPtr<PhysicsInfo> LoadPhysicsInfo(std::string_view name);
        AsyncPtr<HullSettings> LoadHullSettings(std::string_view modelName, std::string_view meshName);
        AsyncPtr<Image> LoadImage(std::string_view path);

        void RegisterExternalGltf(std::string_view name, std::string_view path);
        bool IsGltfRegistered(std::string_view name);

        bool InputStream(std::string_view path, AssetType type, std::ifstream &stream, size_t *size = nullptr);
        bool OutputStream(const std::filesystem::path &path, std::ofstream &stream);

    private:
        void Frame() override;

        void UpdateBundleIndex();
        AssetPath FindGltfByName(std::string_view name);
        AssetPath FindPhysicsByName(std::string_view name);

        std::filesystem::path assetsPath;

        std::atomic_bool shutdown;
        DispatchQueue workQueue;

        std::mutex assetMutex;
        std::mutex gltfMutex;
        std::mutex physicsInfoMutex;
        std::mutex imageMutex;

        EnumArray<PreservingMap<AssetPath, Async<Asset>, 10000, StringHash, StringEqual>, AssetType> loadedAssets;
        PreservingMap<AssetName, Async<Gltf>, 10000, StringHash, StringEqual> loadedGltfs;
        PreservingMap<AssetName, Async<PhysicsInfo>, 10000, StringHash, StringEqual> loadedPhysics;
        PreservingMap<AssetPath, Async<Image>, 10000, StringHash, StringEqual> loadedImages;

        std::mutex externalGltfMutex;
        robin_hood::unordered_flat_map<AssetName, AssetPath, StringHash, StringEqual> externalGltfPaths;

        robin_hood::unordered_flat_map<AssetPath, std::pair<size_t, size_t>, StringHash, StringEqual> bundleIndex;
    };

    AssetManager &Assets();
} // namespace sp
