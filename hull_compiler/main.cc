#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/Async.hh"
#include "assets/Gltf.hh"
#include "assets/PhysicsInfo.hh"
#include "physx/ConvexHull.hh"

#include <PxPhysicsAPI.h>
#include <cxxopts.hpp>
#include <extensions/PxDefaultAllocator.h>
#include <extensions/PxDefaultErrorCallback.h>
#include <filesystem>
#include <fstream>

using namespace sp;

int main(int argc, char **argv) {
    cxxopts::Options options("hull_compiler", "");
    options.positional_help("<model_name>");
    options.add_options()("model-name", "", cxxopts::value<std::string>());
    options.parse_positional({"model-name"});

    auto optionsResult = options.parse(argc, argv);

    if (!optionsResult.count("model-name")) {
        std::cout << options.help() << std::endl;
        return 1;
    }

    std::string modelName = optionsResult["model-name"].as<std::string>();

    auto modelPtr = sp::GAssets.LoadGltf(modelName);
    auto model = modelPtr->Get();
    if (!model) {
        Errorf("hull_compiler could not load Gltf model: %s", modelName);
        return 1;
    }

    auto physicsInfo = sp::GAssets.LoadPhysicsInfo(modelName)->Get();

    physx::PxDefaultErrorCallback defaultErrorCallback;
    physx::PxDefaultAllocator defaultAllocatorCallback;
    auto pxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocatorCallback, defaultErrorCallback);

    physx::PxTolerancesScale scale;
    auto pxPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *pxFoundation, scale);
    Assert(pxPhysics, "PxCreatePhysics");

    auto pxCooking = PxCreateCooking(PX_PHYSICS_VERSION, *pxFoundation, physx::PxCookingParams(scale));
    Assert(pxCooking, "PxCreateCooking");

    auto pxSerialization = physx::PxSerialization::createSerializationRegistry(*pxPhysics);
    Assert(pxSerialization, "PxSerialization::createSerializationRegistry");

    bool updated = false;
    if (physicsInfo) {
        for (auto &[meshName, settings] : physicsInfo->GetHulls()) {
            auto settingsPtr = sp::GAssets.LoadHullSettings(modelName, meshName);

            auto set = hullgen::LoadCollisionCache(*pxSerialization, modelPtr, settingsPtr);
            if (set) continue;

            Logf("Updating physics collision cache: %s.%s", modelName, meshName);

            set = hullgen::BuildConvexHulls(*pxCooking, *pxPhysics, modelPtr, settingsPtr);
            hullgen::SaveCollisionCache(*pxSerialization, modelPtr, settingsPtr, *set);

            updated = true;
        }
    }

    for (size_t i = 0; i < model->meshes.size(); i++) {
        std::string meshName("convex" + std::to_string(i));
        auto settingsPtr = sp::GAssets.LoadHullSettings(modelName, meshName);

        auto set = hullgen::LoadCollisionCache(*pxSerialization, modelPtr, settingsPtr);
        if (set) continue;

        Logf("Updating physics collision cache: %s.%s", modelName, meshName);

        set = hullgen::BuildConvexHulls(*pxCooking, *pxPhysics, modelPtr, settingsPtr);
        hullgen::SaveCollisionCache(*pxSerialization, modelPtr, settingsPtr, *set);

        updated = true;
    }

    std::filesystem::path markerPath("../assets/cache/collision/" + modelName);
    if (updated || !std::filesystem::exists(markerPath)) {
        std::filesystem::create_directories(markerPath.parent_path());
        std::ofstream(markerPath).close(); // Create or touch the marker file
    }
    return 0;
}
