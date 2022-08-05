#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "assets/PhysicsInfo.hh"
#include "physx/ConvexHull.hh"

#include <PxPhysicsAPI.h>
#include <cxxopts.hpp>
#include <extensions/PxDefaultAllocator.h>
#include <extensions/PxDefaultErrorCallback.h>

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

    auto physicsInfo = sp::GAssets.LoadPhysicsInfo(modelName)->Get();
    Logf("Physics name: %s", physicsInfo->modelName);

    auto model = sp::GAssets.LoadGltf(modelName)->Get();
    Logf("Model name: %s", model->name);

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

    for (auto &[hullName, hull] : physicsInfo->GetHulls()) {
        Logf("%s: %s = %s %u", modelName, hullName, hull.name, hull.decompose);

        auto set = hullgen::LoadCollisionCache(*pxSerialization, *model, hull);
        if (set) continue;

        set = hullgen::BuildConvexHulls(*pxCooking, *pxPhysics, *model, hull);
        if (set->hulls.empty()) continue;

        hullgen::SaveCollisionCache(*pxSerialization, *model, hull, *set);
    }
    return 0;
}
