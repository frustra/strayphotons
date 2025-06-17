/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "DynamicScript.hh"

namespace ecs {
    DynamicScriptContext::DynamicScriptContext(const std::shared_ptr<DynamicScript> &script)
        : context(nullptr), script(nullptr) {
        if (script && script->newContextFunc) {
            Assertf(script->freeContextFunc,
                "Cannot construct context for %s(%s) without sp_script_free_context()",
                script->name,
                script->definition.name);
            context = script->newContextFunc(nullptr);
            this->script = script;
        }
    }

    DynamicScriptContext::DynamicScriptContext(const DynamicScriptContext &other) : context(nullptr), script(nullptr) {
        if (other.script && other.context) {
            context = other.script->newContextFunc(other.context);
            script = other.script;
        }
    }

    DynamicScriptContext::~DynamicScriptContext() {
        if (script && context) {
            script->freeContextFunc(context);
            context = nullptr;
        }
        script.reset();
    }

    DynamicScriptContext &DynamicScriptContext::operator=(const DynamicScriptContext &other) {
        if (context == other.context) return *this;
        if (script && context) {
            script->freeContextFunc(context);
            context = nullptr;
        }
        if (other.script && other.context) {
            context = other.script->newContextFunc(other.context);
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
            ZoneStr(dynamicScript->name);
            auto &ptr = dynamicScript->MaybeAllocContext(state);
            if (dynamicScript->initFunc) dynamicScript->initFunc(ptr.context, &state);
        }
    }

    void DynamicScript::Destroy(ScriptState &state) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->name);
            auto *ptr = std::any_cast<DynamicScriptContext>(&state.scriptData);
            if (ptr && dynamicScript->destroyFunc) dynamicScript->destroyFunc(ptr->context, &state);
        }
    }

    void DynamicScript::OnTick(ScriptState &state,
        const DynamicLock<ReadSignalsLock> &lock,
        Entity ent,
        chrono_clock::duration interval) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->name);
            auto &ptr = dynamicScript->MaybeAllocContext(state);
            if (dynamicScript->onTickFunc) {
                ecs::DynamicLock<> dynLock = lock;
                dynamicScript->onTickFunc(ptr.context,
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
            ZoneStr(dynamicScript->name);
            auto &ptr = dynamicScript->MaybeAllocContext(state);
            if (dynamicScript->onEventFunc) {
                ecs::DynamicLock<> dynLock = lock;
                dynamicScript->onEventFunc(ptr.context, &state, &dynLock, (uint64_t)ent, &event);
            }
        }
    }

    void DynamicScript::Prefab(const ScriptState &state, const sp::SceneRef &scene, Lock<AddRemove> lock, Entity ent) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->name);
            if (dynamicScript->prefabFunc) {
                ecs::DynamicLock<> dynLock = lock;
                dynamicScript->prefabFunc(&state, &dynLock, (uint64_t)ent, &scene);
            }
        }
    }

    DynamicScript::DynamicScript(const std::string &name, dynalo::library &&lib, const ScriptDefinition &definition)
        : ScriptDefinitionBase(this->metadata), name(name),
          metadata(typeid(void), 0, definition.name.c_str(), "DynamicScript"), definition(definition),
          dynamicLib(std::move(lib)) {
        switch (definition.type) {
        case ScriptType::LogicScript:
        case ScriptType::PhysicsScript:
            this->definition.initFunc = ScriptInitFunc(&Init);
            this->definition.destroyFunc = ScriptDestroyFunc(&Destroy);
            this->definition.callback = OnTickFunc(&OnTick);
            break;
        case ScriptType::EventScript:
            this->definition.initFunc = ScriptInitFunc(&Init);
            this->definition.destroyFunc = ScriptDestroyFunc(&Destroy);
            this->definition.callback = OnEventFunc(&OnEvent);
            break;
        case ScriptType::PrefabScript:
            this->definition.callback = PrefabFunc(&Prefab);
            break;
        default:
            Abortf("DynamicScript %s(%s) unexpected script type: %s", name, definition.name, definition.type);
        }
    }

    void DynamicScript::Register() const {
        GetScriptDefinitions().RegisterScript(ScriptDefinition(this->definition));
    }

    void DynamicScript::Reload() {
        ZoneScoped;
        ZoneStr(name);
        defaultContext = {}; // Free context with old library version
        dynamicLib.reset();
        auto newScript = Load(name);
        if (!newScript) {
            Errorf("Failed to reload %s(%s)", dynalo::to_native_name(name), definition.name);
            definition.context.reset();
            newContextFunc = nullptr;
            freeContextFunc = nullptr;
            initFunc = nullptr;
            destroyFunc = nullptr;
            onTickFunc = nullptr;
            onEventFunc = nullptr;
            prefabFunc = nullptr;
            return;
        }
        dynamicLib = std::move(newScript->dynamicLib);
        definition = newScript->definition;
        definition.context = newScript;
        defaultContext = DynamicScriptContext(newScript);
        newContextFunc = newScript->newContextFunc;
        freeContextFunc = newScript->freeContextFunc;
        initFunc = newScript->initFunc;
        destroyFunc = newScript->destroyFunc;
        onTickFunc = newScript->onTickFunc;
        onEventFunc = newScript->onEventFunc;
        prefabFunc = newScript->prefabFunc;
    }

    std::shared_ptr<DynamicScript> DynamicScript::Load(const std::string &name) {
        ZoneScoped;
        ZoneStr(name);
        auto nativeName = dynalo::to_native_name(name);
        dynalo::library dynamicLib("./" + nativeName);
        ScriptDefinition definition{};

        auto definitionFunc = dynamicLib.get_function<size_t(ScriptDefinition *)>("sp_script_get_definition");
        if (!definitionFunc) {
            Errorf("Failed to load %s, sp_script_get_definition() is missing", nativeName);
            return nullptr;
        }
        definitionFunc(&definition);

        std::shared_ptr<DynamicScript> scriptPtr = std::shared_ptr<DynamicScript>(
            new DynamicScript(name, std::move(dynamicLib), definition));

        DynamicScript &script = *scriptPtr;
        script.definition.context = scriptPtr;
        script.LoadFunc(script.newContextFunc, "sp_script_new_context");
        script.LoadFunc(script.freeContextFunc, "sp_script_free_context");
        script.LoadFunc(script.initFunc, "sp_script_init");
        script.LoadFunc(script.destroyFunc, "sp_script_destroy");
        script.LoadFunc(script.onTickFunc, "sp_script_on_tick");
        script.LoadFunc(script.onEventFunc, "sp_script_on_event");
        script.LoadFunc(script.prefabFunc, "sp_script_prefab");

        switch (definition.type) {
        case ScriptType::LogicScript:
        case ScriptType::PhysicsScript:
            if (!script.onTickFunc) {
                Errorf("Failed to load %s(%s), %s is missing sp_script_on_tick()",
                    nativeName,
                    definition.name,
                    definition.type);
                return nullptr;
            }
            break;
        case ScriptType::EventScript:
            if (!script.onEventFunc) {
                Errorf("Failed to load %s(%s), EventScript is missing sp_script_on_event()",
                    nativeName,
                    definition.name);
                return nullptr;
            }
            break;
        case ScriptType::PrefabScript:
            if (!script.prefabFunc) {
                Errorf("Failed to load %s(%s), PrefabScript is missing sp_script_prefab()",
                    nativeName,
                    definition.name);
                return nullptr;
            }
            if (script.initFunc)
                Warnf("%s(%s) PrefabScript defines unsupported sp_script_init()", nativeName, definition.name);
            if (script.destroyFunc)
                Warnf("%s(%s) PrefabScript defines unsupported sp_script_destroy()", nativeName, definition.name);
            break;
        default:
            Errorf("DynamicScript %s(%s) unexpected script type: %s", nativeName, definition.name, definition.type);
            return nullptr;
        }

        script.defaultContext = DynamicScriptContext(scriptPtr);
        return scriptPtr;
    }
} // namespace ecs
