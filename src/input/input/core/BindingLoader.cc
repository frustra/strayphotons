#include "BindingLoader.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "core/Console.hh"
#include "ecs/EcsImpl.hh"
#include "input/core/BindingNames.hh"
#include "input/core/KeyCodes.hh"

#include <filesystem>
#include <fstream>
#include <picojson/picojson.h>

namespace sp {
    BindingLoader::BindingLoader() : consoleInputEntity("console-input"), keyboardEntity("keyboard") {
        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

            auto ent = consoleInputEntity.Get(lock);
            if (!ent) {
                Logf("Creating console-input binding entity");
                ent = lock.NewEntity();
                ent.Set<ecs::Owner>(lock, ecs::Owner::SystemId::INPUT_MANAGER);
                ent.Set<ecs::Name>(lock, consoleInputEntity.Name());
                ent.Set<ecs::FocusLayer>(lock, ecs::FocusLayer::GAME);
                consoleInputEntity = ecs::NamedEntity(consoleInputEntity.Name(), ent);
            }
            ent.Get<ecs::EventInput>(lock);
            auto &script = ent.Get<ecs::Script>(lock);
            script.AddOnTick([](ecs::Lock<ecs::WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
                if (ent.Has<ecs::Script, ecs::EventInput>(lock)) {
                    auto &script = ent.GetPrevious<ecs::Script>(lock);
                    auto &readInput = ent.GetPrevious<ecs::EventInput>(lock);
                    bool hasEvents = false;
                    for (auto &queue : readInput.events) {
                        if (!queue.second.empty()) {
                            hasEvents = true;
                            break;
                        }
                    }
                    if (hasEvents) {
                        auto &input = ent.Get<ecs::EventInput>(lock);
                        for (auto &[name, queue] : input.events) {
                            while (!queue.empty()) {
                                auto command = script.GetParam<std::string>(name);
                                if (!command.empty()) { GetConsoleManager().QueueParseAndExecute(command); }
                                queue.pop();
                            }
                        }
                    }
                }
            });
        }

        funcs.Register(this, "bind", "Bind a key to a command", &BindingLoader::BindKey);
    }

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

            for (auto param : root.get<picojson::object>()) {
                if (param.first == "_console") {
                    Debugf("Loading console-input bindings");
                    for (auto eventInput : param.second.get<picojson::object>()) {
                        auto command = eventInput.second.get<std::string>();
                        SetConsoleInputCommand(lock, eventInput.first, command);
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
    }

    void BindingLoader::SetConsoleInputCommand(
        ecs::Lock<ecs::Read<ecs::Name>, ecs::Write<ecs::Script, ecs::EventInput>> lock,
        std::string eventName,
        std::string command) {
        auto consoleInput = consoleInputEntity.Get(lock);
        if (!consoleInput.Has<ecs::Script, ecs::EventInput>(lock)) Abort("Console input entity is invalid");

        auto &eventInput = consoleInput.Get<ecs::EventInput>(lock);
        if (!eventInput.IsRegistered(eventName)) eventInput.Register(eventName);
        auto &script = consoleInput.Get<ecs::Script>(lock);
        script.SetParam(eventName, command);
    }

    void BindingLoader::BindKey(string args) {
        std::stringstream stream(args);
        std::string keyName;
        stream >> keyName;

        to_lower(keyName);
        auto it = UserBindingAliases.find(keyName);
        if (it != UserBindingAliases.end()) keyName = it->second;

        KeyCode key = KEY_INVALID;
        for (auto &[keyCode, name] : KeycodeNameLookup) {
            if (name == keyName) {
                key = (KeyCode)keyCode;
                break;
            }
        }
        if (key != KEY_INVALID) {
            std::string command;
            std::getline(stream, command);
            trim(command);

            {
                auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>,
                                                        ecs::Write<ecs::Script, ecs::EventInput, ecs::EventBindings>>();

                auto keyboard = keyboardEntity.Get(lock);
                if (!keyboard.Has<ecs::EventBindings>(lock)) {
                    Errorf("Can't bind key without valid keyboard entity");
                    return;
                }

                Logf("Binding %s to command: %s", keyName, command);
                std::string eventName = INPUT_EVENT_KEYBOARD_KEY_BASE + keyName;
                SetConsoleInputCommand(lock, eventName, command);

                auto &bindings = keyboard.Get<ecs::EventBindings>(lock);
                bindings.Unbind(eventName, consoleInputEntity, eventName);
                bindings.Bind(eventName, consoleInputEntity, eventName);
            }
        } else {
            Errorf("Key \"%s\" does not exist", keyName);
        }
    }
} // namespace sp
