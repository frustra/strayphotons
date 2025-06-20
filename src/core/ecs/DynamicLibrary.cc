/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "DynamicLibrary.hh"

#include <dynalo/dynalo.hpp>

namespace ecs {
    void DynamicLibrary::RegisterScripts() const {
        for (auto &script : scripts) {
            script->Register();
        }
    }

    void DynamicLibrary::ReloadLibrary() {
        ZoneScoped;
        ZoneStr(name);
        scripts.clear(); // Free script contexts with old library version
        dynamicLib.reset();
        auto newLibrary = Load(name);
        if (!newLibrary) {
            Errorf("Failed to reload library: %s", dynalo::to_native_name(name));
            return;
        }
        dynamicLib = std::move(newLibrary->dynamicLib);
        scripts = std::move(newLibrary->scripts);
        for (auto &script : scripts) {
            script->library = this;
        }
    }

    std::vector<std::shared_ptr<DynamicScript>> &DynamicLibrary::GetScripts() {
        return scripts;
    }

    std::shared_ptr<DynamicLibrary> DynamicLibrary::Load(const std::string &name) {
        ZoneScoped;
        ZoneStr(name);
        auto nativeName = dynalo::to_native_name(name);
        dynalo::library dynamicLib("./" + nativeName);

        auto getDefinitionsFunc = dynamicLib.get_function<size_t(DynamicScriptDefinition *, size_t)>(
            "sp_library_get_script_definitions");
        if (!getDefinitionsFunc) {
            Errorf("Failed to load %s, sp_library_get_script_definitions() is missing", nativeName);
            return nullptr;
        }
        size_t scriptCount = getDefinitionsFunc(nullptr, 0);
        std::vector<DynamicScriptDefinition> definitions;
        definitions.resize(scriptCount);
        getDefinitionsFunc(definitions.data(), definitions.size());

        std::shared_ptr<DynamicLibrary> libraryPtr(new DynamicLibrary(name, std::move(dynamicLib)));
        DynamicLibrary &library = *libraryPtr;
        for (const auto &definition : definitions) {
            auto script = DynamicScript::Load(library, definition);
            if (script) library.scripts.emplace_back(script);
        }
        return libraryPtr;
    }

    DynamicLibrary::DynamicLibrary(const std::string &name, dynalo::library &&lib)
        : name(name), dynamicLib(std::make_shared<dynalo::library>(std::move(lib))) {}

    DynamicScriptContext::DynamicScriptContext(const std::shared_ptr<DynamicScript> &script)
        : context(nullptr), script(nullptr) {
        if (script && script->dynamicDefinition.newContextFunc) {
            Assertf(script->dynamicDefinition.freeContextFunc,
                "Cannot construct context for %s(%s) without sp_script_free_context()",
                script->library->name,
                script->definition.name);
            context = script->dynamicDefinition.newContextFunc(nullptr);
            this->script = script;
        }
    }

    DynamicScriptContext::DynamicScriptContext(const DynamicScriptContext &other) : context(nullptr), script(nullptr) {
        if (other.script && other.context) {
            context = other.script->dynamicDefinition.newContextFunc(other.context);
            script = other.script;
        }
    }

    DynamicScriptContext::~DynamicScriptContext() {
        if (script && context) {
            script->dynamicDefinition.freeContextFunc(context);
            context = nullptr;
        }
        script.reset();
    }

    DynamicScriptContext &DynamicScriptContext::operator=(const DynamicScriptContext &other) {
        if (context == other.context) return *this;
        if (script && context) {
            script->dynamicDefinition.freeContextFunc(context);
            context = nullptr;
        }
        if (other.script && other.context) {
            context = other.script->dynamicDefinition.newContextFunc(other.context);
        } else {
            context = nullptr;
        }
        script = other.script;
        return *this;
    }

    DynamicScriptContext &DynamicScript::MaybeAllocContext(ScriptState &state) const {
        auto *ptr = std::any_cast<DynamicScriptContext>(&state.scriptData);
        if (!ptr) {
            return state.scriptData.emplace<DynamicScriptContext>(
                std::dynamic_pointer_cast<DynamicScript>(state.definition.context.lock()));
        }
        return *ptr;
    }

    const void *DynamicScript::GetDefault() const {
        return defaultContext.context;
    }

    const void *DynamicScript::Access(const ScriptState &state) const {
        const auto *ptr = std::any_cast<DynamicScriptContext>(&state.scriptData);
        return ptr ? ptr->context : GetDefault();
    }

    void *DynamicScript::AccessMut(ScriptState &state) const {
        auto &ptr = MaybeAllocContext(state);
        return ptr.context;
    }

    void DynamicScript::Init(ScriptState &state) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            auto &ptr = dynamicScript->MaybeAllocContext(state);
            if (dynamicScript->dynamicDefinition.initFunc) {
                dynamicScript->dynamicDefinition.initFunc(ptr.context, &state);
            }
        }
    }

    void DynamicScript::Destroy(ScriptState &state) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            auto *ptr = std::any_cast<DynamicScriptContext>(&state.scriptData);
            if (ptr && dynamicScript->dynamicDefinition.destroyFunc) {
                dynamicScript->dynamicDefinition.destroyFunc(ptr->context, &state);
            }
        }
    }

    void DynamicScript::OnTick(ScriptState &state,
        const DynamicLock<ReadSignalsLock> &lock,
        Entity ent,
        chrono_clock::duration interval) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            auto &ptr = dynamicScript->MaybeAllocContext(state);
            if (dynamicScript->dynamicDefinition.onTickFunc) {
                ecs::DynamicLock<> dynLock = lock;
                dynamicScript->dynamicDefinition.onTickFunc(ptr.context,
                    &state,
                    &dynLock,
                    (uint64_t)ent,
                    std::chrono::nanoseconds(interval).count());
            }
        }
    }

    void DynamicScript::OnEvent(ScriptState &state, const DynamicLock<ReadSignalsLock> &lock, Entity ent, Event event) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            auto &ptr = dynamicScript->MaybeAllocContext(state);
            if (dynamicScript->dynamicDefinition.onEventFunc) {
                ecs::DynamicLock<> dynLock = lock;
                dynamicScript->dynamicDefinition.onEventFunc(ptr.context, &state, &dynLock, (uint64_t)ent, &event);
            }
        }
    }

    void DynamicScript::Prefab(const ScriptState &state, const sp::SceneRef &scene, Lock<AddRemove> lock, Entity ent) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            if (dynamicScript->dynamicDefinition.prefabFunc) {
                ecs::DynamicLock<> dynLock = lock;
                dynamicScript->dynamicDefinition.prefabFunc(&state, &dynLock, (uint64_t)ent, &scene);
            }
        }
    }

    DynamicScript::DynamicScript(DynamicLibrary &library, const DynamicScriptDefinition &dynamicDefinition)
        : ScriptDefinitionBase(this->metadata),
          metadata(typeid(void), 0, dynamicDefinition.name.c_str(), "DynamicScript"), library(&library),
          dynamicDefinition(dynamicDefinition) {
        definition.name = dynamicDefinition.name;
        definition.type = dynamicDefinition.type;
        definition.events = dynamicDefinition.events;
        definition.filterOnEvent = dynamicDefinition.filterOnEvent;
        switch (definition.type) {
        case ScriptType::LogicScript:
        case ScriptType::PhysicsScript:
            definition.initFunc = ScriptInitFunc(&Init);
            definition.destroyFunc = ScriptDestroyFunc(&Destroy);
            definition.callback = OnTickFunc(&OnTick);
            break;
        case ScriptType::EventScript:
            definition.initFunc = ScriptInitFunc(&Init);
            definition.destroyFunc = ScriptDestroyFunc(&Destroy);
            definition.callback = OnEventFunc(&OnEvent);
            break;
        case ScriptType::PrefabScript:
            definition.callback = PrefabFunc(&Prefab);
            break;
        default:
            Abortf("DynamicLibrary %s(%s) unexpected script type: %s", library.name, definition.name, definition.type);
        }
    }

    std::shared_ptr<DynamicScript> DynamicScript::Load(DynamicLibrary &library,
        const DynamicScriptDefinition &definition) {
        ZoneScoped;
        ZoneStr(definition.name);

        switch (definition.type) {
        case ScriptType::LogicScript:
        case ScriptType::PhysicsScript:
            if (!definition.onTickFunc) {
                Errorf("Failed to load %s(%s), %s is missing sp_script_on_tick()",
                    library.name,
                    definition.name,
                    definition.type);
                return nullptr;
            }
            break;
        case ScriptType::EventScript:
            if (!definition.onEventFunc) {
                Errorf("Failed to load %s(%s), EventScript is missing sp_script_on_event()",
                    library.name,
                    definition.name);
                return nullptr;
            }
            break;
        case ScriptType::PrefabScript:
            if (!definition.prefabFunc) {
                Errorf("Failed to load %s(%s), PrefabScript is missing sp_script_prefab()",
                    library.name,
                    definition.name);
                return nullptr;
            }
            if (definition.initFunc)
                Warnf("%s(%s) PrefabScript defines unsupported sp_script_init()", library.name, definition.name);
            if (definition.destroyFunc)
                Warnf("%s(%s) PrefabScript defines unsupported sp_script_destroy()", library.name, definition.name);
            break;
        default:
            Errorf("DynamicLibrary %s(%s) unexpected script type: %s", library.name, definition.name, definition.type);
            return nullptr;
        }

        std::shared_ptr<DynamicScript> scriptPtr(new DynamicScript(library, definition));
        DynamicScript &script = *scriptPtr;
        script.definition.context = scriptPtr;
        script.defaultContext = DynamicScriptContext(scriptPtr);
        return scriptPtr;
    }

    void DynamicScript::Register() const {
        GetScriptDefinitions().RegisterScript(ScriptDefinition(definition));
    }
} // namespace ecs
