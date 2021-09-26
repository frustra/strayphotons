#include "BindingLoader.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "ecs/EcsImpl.hh"

#include <filesystem>
#include <fstream>
#include <picojson/picojson.h>

namespace sp {
    BindingLoader::BindingLoader() {}

    void BindingLoader::Load(std::string bindingConfigPath) {
        std::string bindingConfig;

        if (!std::filesystem::exists(bindingConfigPath)) {
            auto defaultConfigAsset = GAssets.Load("default_input_bindings.json");
            Assert(defaultConfigAsset, "Default input binding config missing");
            defaultConfigAsset->WaitUntilValid();

            bindingConfig = defaultConfigAsset->String();

            std::ofstream file(bindingConfigPath, std::ios::binary);
            Assert(file.is_open(), "Failed to create binding config file: " + bindingConfigPath);
            file << bindingConfig;
            file.close();
        } else {
            std::ifstream file(bindingConfigPath);
            Assert(file.is_open(), "Failed to open binding config file: " + bindingConfigPath);

            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);

            bindingConfig.resize(size);
            file.read(bindingConfig.data(), size);
            file.close();
        }

        picojson::value root;
        string err = picojson::parse(root, bindingConfig);
        if (!err.empty()) Abort("Failed to parse user input_binding.json file: " + err);

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            for (auto entityParam : root.get<picojson::object>()) {
                auto entity = ecs::EntityWith<ecs::Name>(lock, entityParam.first);
                if (entity) {
                    Debugf("Loading input for: %s", entityParam.first);
                    for (auto comp : entityParam.second.get<picojson::object>()) {
                        if (comp.first[0] == '_') continue;

                        auto componentType = ecs::LookupComponent(comp.first);
                        if (componentType != nullptr) {
                            bool result = componentType->ReloadEntity(lock, entity, comp.second);
                            Assert(result, "Failed to load component type: " + comp.first);
                        } else {
                            Errorf("Unknown component, ignoring: %s", comp.first);
                        }
                    }
                }
            }
        }
    }
} // namespace sp
