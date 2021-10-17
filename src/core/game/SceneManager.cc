#include "SceneManager.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/Scene.hh"
#include "core/Logging.hh"
#include "core/console/Console.hh"
#include "ecs/EcsImpl.hh"

#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <picojson/picojson.h>

namespace sp {
    SceneManager::SceneManager() {
        funcs.Register(this, "loadscene", "Load a scene", &SceneManager::LoadScene);
        funcs.Register(this, "reloadscene", "Reload current scene", &SceneManager::ReloadScene);
        funcs.Register(this, "reloadplayer", "Reload player scene", &SceneManager::ReloadPlayer);
        funcs.Register(this, "printscene", "Print info about currently scenes", &SceneManager::PrintScene);
    }

    void SceneManager::LoadPlayer() {
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            ecs::DestroyAllWith<ecs::Owner>(lock, ecs::Owner(ecs::Owner::OwnerType::PLAYER, 0));
        }

        if (playerScene != nullptr) {
            for (auto &line : playerScene->unloadExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }

        playerScene.reset();
        player = Tecs::Entity();

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            playerScene = LoadSceneJson("player", lock, ecs::Owner(ecs::Owner::OwnerType::PLAYER, 0));

            for (auto e : lock.EntitiesWith<ecs::Name>()) {
                if (e.Has<ecs::Owner>(lock)) {
                    auto &name = e.Get<ecs::Name>(lock);
                    auto &owner = e.Get<ecs::Owner>(lock);
                    if (owner == ecs::Owner(ecs::Owner::OwnerType::PLAYER, 0)) {
                        if (name == "player") {
                            player = e;
                            break;
                        }
                    }
                }
            }
            Assert(!!player, "Player scene doesn't contain an entity named player");

            auto spawn = ecs::EntityWith<ecs::Name>(lock, "player-spawn");
            auto vrOrigin = ecs::EntityWith<ecs::Name>(lock, "vr-origin");
            if (spawn.Has<ecs::Transform>(lock)) {
                auto &spawnTransform = spawn.Get<ecs::Transform>(lock);
                auto spawnPosition = spawnTransform.GetGlobalPosition(lock);
                auto spawnRotation = spawnTransform.GetGlobalRotation(lock);
                auto &playerTransform = player.Get<ecs::Transform>(lock);
                playerTransform.SetPosition(spawnPosition);
                playerTransform.SetRotation(spawnRotation);
                playerTransform.UpdateCachedTransform(lock);
                if (vrOrigin) {
                    auto &vrTransform = vrOrigin.Get<ecs::Transform>(lock);
                    vrTransform.SetPosition(spawnPosition);
                    vrTransform.SetRotation(spawnRotation);
                    vrTransform.UpdateCachedTransform(lock);
                }
            } else if (!player.Has<ecs::Transform>(lock)) {
                player.Set<ecs::Transform>(lock, glm::vec3(0));
            }

            // Mark the player as being able to activate trigger areas
            player.Set<ecs::Triggerable>(lock);
            // Allow the player to be a parent for constraints
            player.Set<ecs::InteractController>(lock);
        }

        if (playerScene) {
            for (auto &line : playerScene->autoExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }
    }

    shared_ptr<Scene> SceneManager::LoadSceneJson(const std::string &sceneName,
                                                  ecs::Lock<ecs::AddRemove> lock,
                                                  ecs::Owner owner) {
        Logf("Loading scene: %s", sceneName);

        std::shared_ptr<const Asset> asset = GAssets.Load("scenes/" + sceneName + ".json", true);
        if (!asset) {
            Logf("Scene not found");
            return nullptr;
        }
        asset->WaitUntilValid();
        picojson::value root;
        string err = picojson::parse(root, asset->String());
        if (!err.empty()) {
            Errorf("%s", err);
            return nullptr;
        }

        shared_ptr<Scene> scene = make_shared<Scene>(sceneName, asset);

        auto autoExecList = root.get<picojson::object>()["autoexec"];
        if (autoExecList.is<picojson::array>()) {
            for (auto value : autoExecList.get<picojson::array>()) {
                auto line = value.get<string>();
                scene->autoExecList.push_back(line);
            }
        }

        auto unloadExecList = root.get<picojson::object>()["unloadexec"];
        if (unloadExecList.is<picojson::array>()) {
            for (auto value : unloadExecList.get<picojson::array>()) {
                auto line = value.get<string>();
                scene->unloadExecList.push_back(line);
            }
        }

        std::unordered_map<std::string, Tecs::Entity> namedEntities;

        auto entityList = root.get<picojson::object>()["entities"];
        // Find all named entities first so they can be referenced.
        for (auto value : entityList.get<picojson::array>()) {
            auto ent = value.get<picojson::object>();

            if (ent.count("_name")) {
                Tecs::Entity entity = lock.NewEntity();
                auto name = ent["_name"].get<string>();
                entity.Set<ecs::Name>(lock, name);
                Assert(namedEntities.count(name) == 0, "Duplicate entity name: " + name);
                namedEntities.emplace(name, entity);
            }
        }

        for (auto value : entityList.get<picojson::array>()) {
            auto ent = value.get<picojson::object>();

            Tecs::Entity entity;
            if (ent.count("_name")) {
                entity = namedEntities[ent["_name"].get<string>()];
            } else {
                entity = lock.NewEntity();
            }

            entity.Set<ecs::Owner>(lock, owner);
            for (auto comp : ent) {
                if (comp.first[0] == '_') continue;

                auto componentType = ecs::LookupComponent(comp.first);
                if (componentType != nullptr) {
                    bool result = componentType->LoadEntity(lock, entity, comp.second);
                    Assert(result, "Failed to load component type: " + comp.first);
                } else {
                    Errorf("Unknown component, ignoring: %s", comp.first);
                }
            }
            scene->entities.push_back(entity);
        }
        return scene;
    }

    std::shared_ptr<Scene> SceneManager::LoadBindingJson(std::string bindingConfigPath) {
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

            for (auto param : root.get<picojson::object>()) {
                if (param.first == "_console") {
                    Debugf("Loading console-input bindings");
                    for (auto eventInput : param.second.get<picojson::object>()) {
                        auto command = eventInput.second.get<std::string>();
                        consoleBinding.SetConsoleInputCommand(lock, eventInput.first, command);
                    }
                } else {
                    auto entity = ecs::EntityWith<ecs::Name>(lock, param.first);
                    if (entity) {
                        Debugf("Loading input for: %s", param.first);
                        for (auto comp : param.second.get<picojson::object>()) {
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

        // TODO
        return nullptr;
    }

    void SceneManager::LoadScene(std::string sceneName) {
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            ecs::DestroyAllWith<ecs::Owner>(lock, ecs::Owner(ecs::Owner::OwnerType::SCENE, 0));
        }

        if (scene != nullptr) {
            for (auto &line : scene->unloadExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }

        scene.reset();
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            scene = LoadSceneJson(sceneName, lock, ecs::Owner(ecs::Owner::OwnerType::SCENE, 0));

            for (auto e : lock.EntitiesWith<ecs::Name>()) {
                auto &name = e.Get<ecs::Name>(lock);
                if (name == "player" && e != player) {
                    name = "player-spawn";
                    if (e.Has<ecs::Transform>(lock)) {
                        auto &spawnTransform = e.Get<ecs::Transform>(lock);
                        auto spawnPosition = spawnTransform.GetGlobalPosition(lock);
                        auto spawnRotation = spawnTransform.GetGlobalRotation(lock);
                        auto &playerTransform = player.Get<ecs::Transform>(lock);
                        playerTransform.SetPosition(spawnPosition);
                        playerTransform.SetRotation(spawnRotation);
                        playerTransform.UpdateCachedTransform(lock);

                        auto vrOrigin = ecs::EntityWith<ecs::Name>(lock, "vr-origin");
                        if (vrOrigin) {
                            auto &vrTransform = vrOrigin.Get<ecs::Transform>(lock);
                            vrTransform.SetPosition(spawnPosition);
                            vrTransform.SetRotation(spawnRotation);
                            vrTransform.UpdateCachedTransform(lock);
                        }
                    }
                    break;
                }
            }
        }

        LoadBindingJson(InputBindingConfigPath);

        if (scene) {
            for (auto &line : scene->autoExecList) {
                GetConsoleManager().ParseAndExecute(line);
            }
        }
    }

    void SceneManager::ReloadScene() {
        if (scene) LoadScene(scene->name);
    }

    void SceneManager::ReloadPlayer() {
        LoadPlayer();

        // TODO: Find a way to handle entity overrides / layers in a generic way
#ifdef SP_XR_SUPPORT
        if (game->options["no-vr"].count() == 0) game->xr.LoadXrSystem();
#endif

        LoadBindingJson(InputBindingConfigPath);
    }

    void SceneManager::PrintScene() {
        Logf("Currently loaded scene: %s", scene ? scene->name : "none");
    }
} // namespace sp
