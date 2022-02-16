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

namespace tinygltf {
    struct FsCallbacks;
}

namespace sp {
    class Asset;
    class Model;
    class Script;
    class Image;

    enum class AssetType {
        Bundled = 0,
        External,
        Count,
    };

    class AssetManager : public RegisteredThread {
    public:
        AssetManager();
        ~AssetManager();

        AsyncPtr<Asset> Load(const std::string &path, AssetType type = AssetType::Bundled, bool reload = false);
        AsyncPtr<Model> LoadModel(const std::string &name);
        AsyncPtr<Image> LoadImage(const std::string &path);

        AsyncPtr<Script> LoadScript(const std::string &path);

        void RegisterExternalModel(const std::string &name, const std::string &path);
        bool IsModelRegistered(const std::string &name);

    private:
        void Frame() override;

        void UpdateTarIndex();

        bool InputStream(const std::string &path, AssetType type, std::ifstream &stream, size_t *size = nullptr);
        bool OutputStream(const std::string &path, std::ofstream &stream);

        static bool ReadWholeFile(std::vector<unsigned char> *out,
            std::string *err,
            const std::string &path,
            void *userdata);

        // TODO: Update PhysxManager to use Asset object for collision model cache
        friend class PhysxManager;

        DispatchQueue workQueue;

        std::mutex assetMutex;
        std::mutex modelMutex;
        std::mutex imageMutex;

        EnumArray<PreservingMap<std::string, Async<Asset>>, AssetType> loadedAssets;
        PreservingMap<std::string, Async<Model>> loadedModels;
        PreservingMap<std::string, Async<Image>> loadedImages;

        std::mutex externalModelMutex;
        robin_hood::unordered_flat_map<std::string, std::string> externalModelPaths;

        std::unique_ptr<tinygltf::FsCallbacks> gltfLoaderCallbacks;
        robin_hood::unordered_flat_map<std::string, std::pair<size_t, size_t>> tarIndex;
    };

    extern AssetManager GAssets;
} // namespace sp
