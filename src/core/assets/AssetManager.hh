#pragma once

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

    class AssetManager : public RegisteredThread {
    public:
        AssetManager();

        std::shared_ptr<const Asset> Load(const std::string &path, bool reload = false);
        std::shared_ptr<const Model> LoadModel(const std::string &name);
        std::shared_ptr<const Image> LoadImage(const std::string &path);

        std::shared_ptr<Script> LoadScript(const std::string &path);

    private:
        void Frame() override;

        void UpdateTarIndex();

        bool InputStream(const std::string &path, std::ifstream &stream, size_t *size = nullptr);
        bool OutputStream(const std::string &path, std::ofstream &stream);

        static bool ReadWholeFile(std::vector<unsigned char> *out,
            std::string *err,
            const std::string &path,
            void *userdata);

        // TODO: Update PhysxManager to use Asset object for collision model cache
        friend class PhysxManager;

        std::mutex taskMutex;
        std::vector<std::future<void>> runningTasks;

        std::mutex assetMutex;
        std::mutex modelMutex;
        std::mutex imageMutex;

        PreservingMap<std::string, Asset> loadedAssets;
        PreservingMap<std::string, Model> loadedModels;
        PreservingMap<std::string, Image> loadedImages;

        std::unique_ptr<tinygltf::FsCallbacks> gltfLoaderCallbacks;
        robin_hood::unordered_flat_map<std::string, std::pair<size_t, size_t>> tarIndex;
    };

    extern AssetManager GAssets;
} // namespace sp
