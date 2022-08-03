#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "assets/PhysicsInfo.hh"
#include "physx/ConvexHull.hh"

#include <cxxopts.hpp>

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

    for (auto &[hullName, hull] : physicsInfo->GetHulls()) {
        Logf("%s: %s = %s %u", modelName, hullName, hull.name, hull.decompose);
        auto hullSet = hullgen::BuildConvexHulls(*model, hull);
        (void)hullSet;
    }
    return 0;
}
