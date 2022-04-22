#pragma once

#include "assets/Async.hh"
#include "core/DispatchQueue.hh"
#include "core/EnumArray.hh"
#include "core/PreservingMap.hh"
#include "core/RegisteredThread.hh"
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
    class ConsoleScript;
    class Image;

    enum class AssetType {
        Bundled = 0,
        External,
        Count,
    };

    class AssetManager : public RegisteredThread {
    public:
        AssetManager();

        AsyncPtr<Asset> Load(const std::string &path, AssetType type = AssetType::Bundled, bool reload = false);
        AsyncPtr<Gltf> LoadGltf(const std::string &name);
        AsyncPtr<Image> LoadImage(const std::string &path);

        AsyncPtr<ConsoleScript> LoadScript(const std::string &path);

        void RegisterExternalGltf(const std::string &name, const std::string &path);
        bool IsGltfRegistered(const std::string &name);

    private:
        void Frame() override;

        void UpdateTarIndex();

        bool InputStream(const std::string &path, AssetType type, std::ifstream &stream, size_t *size = nullptr);
        bool OutputStream(const std::string &path, std::ofstream &stream);

        // TODO: Update PhysxManager to use Asset object for collision model cache
        friend class PhysxManager;

        DispatchQueue workQueue;

        std::mutex assetMutex;
        std::mutex gltfMutex;
        std::mutex imageMutex;

        EnumArray<PreservingMap<std::string, Async<Asset>>, AssetType> loadedAssets;
        PreservingMap<std::string, Async<Gltf>> loadedGltfs;
        PreservingMap<std::string, Async<Image>> loadedImages;

        std::mutex externalGltfMutex;
        robin_hood::unordered_flat_map<std::string, std::string> externalGltfPaths;

        robin_hood::unordered_flat_map<std::string, std::pair<size_t, size_t>> tarIndex;
    };

    extern AssetManager GAssets;
} // namespace sp
