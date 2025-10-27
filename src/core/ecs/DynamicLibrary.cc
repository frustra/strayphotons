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
        scripts.clear(); // Free script contexts with old plugin library version
        dynamicLib.reset();
        auto newLibrary = Load(name);
        if (!newLibrary) {
            Errorf("Failed to reload plugin library: %s", dynalo::to_native_name(name));
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
        dynalo::library dynamicLib("./plugins/" + nativeName);
        if (!dynamicLib.get_native_handle()) {
            Errorf("Failed to load %s: %s", nativeName, dynalo::detail::last_error());
            return nullptr;
        }

        auto getDefinitionsFunc = dynamicLib.get_function<size_t(DynamicScriptDefinition *, size_t)>(
            "sp_plugin_get_script_definitions");
        if (!getDefinitionsFunc) {
            Errorf("Failed to load %s, sp_plugin_get_script_definitions() is missing", nativeName);
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
        : context(nullptr), script(script) {
        if (script && script->dynamicDefinition.contextSize > 0) {
            context = std::malloc(script->dynamicDefinition.contextSize);
            std::memset(context, 0, script->dynamicDefinition.contextSize);
            if (script->dynamicDefinition.defaultInitFunc) {
                script->dynamicDefinition.defaultInitFunc(context);
            }
        }
    }

    DynamicScriptContext::DynamicScriptContext(const DynamicScriptContext &other) : context(nullptr), script(nullptr) {
        if (other.script && other.context) {
            Assertf(!other.script->dynamicDefinition.defaultFreeFunc,
                "Cannot copy script context due to defined free func: %s",
                other.script->definition.name);
            context = std::malloc(other.script->dynamicDefinition.contextSize);
            std::memcpy(context, other.context, other.script->dynamicDefinition.contextSize);
            script = other.script;
        }
    }

    DynamicScriptContext::~DynamicScriptContext() {
        if (script && context) {
            if (script->dynamicDefinition.defaultFreeFunc) script->dynamicDefinition.defaultFreeFunc(context);
            std::free(context);
            context = nullptr;
        }
        script.reset();
    }

    DynamicScriptContext &DynamicScriptContext::operator=(const DynamicScriptContext &other) {
        if (context == other.context) return *this;
        if (script && context) {
            if (script->dynamicDefinition.defaultFreeFunc) script->dynamicDefinition.defaultFreeFunc(context);
            std::free(context);
            context = nullptr;
        }
        if (other.script && other.context) {
            Assertf(!other.script->dynamicDefinition.defaultFreeFunc,
                "Cannot copy script context due to defined free func: %s",
                other.script->definition.name);
            context = std::malloc(other.script->dynamicDefinition.contextSize);
            std::memcpy(context, other.context, other.script->dynamicDefinition.contextSize);
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
                Logf("Core int32: %llx, state: %llx", &typeid(int32_t), &typeid(ScriptState));
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
                DynamicLock<> dynLock = lock;
                dynamicScript->dynamicDefinition.onTickFunc(ptr.context,
                    &state,
                    &dynLock,
                    ent,
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
                DynamicLock<> dynLock = lock;
                dynamicScript->dynamicDefinition.onEventFunc(ptr.context, &state, &dynLock, ent, &event);
            }
        }
    }

    void DynamicScript::Prefab(const ScriptState &state, const sp::SceneRef &scene, Lock<AddRemove> lock, Entity ent) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            if (dynamicScript->dynamicDefinition.prefabFunc) {
                DynamicLock<> dynLock = lock;
                dynamicScript->dynamicDefinition.prefabFunc(&state, &dynLock, ent, &scene);
            }
        }
    }

    void DynamicScript::BeforeFrame(ScriptState &state, const DynamicLock<SendEventsLock> &lock, Entity ent) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            auto &ptr = dynamicScript->MaybeAllocContext(state);
            if (dynamicScript->dynamicDefinition.beforeFrameFunc) {
                ecs::DynamicLock<> dynLock = lock;
                dynamicScript->dynamicDefinition.beforeFrameFunc(ptr.context, &state, &dynLock, ent);
            }
        }
    }

    ImDrawData *DynamicScript::RenderGui(ScriptState &state,
        Entity ent,
        glm::vec2 displaySize,
        glm::vec2 scale,
        float deltaTime) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            auto &ptr = dynamicScript->MaybeAllocContext(state);
            if (dynamicScript->dynamicDefinition.renderGuiFunc) {
                auto &renderGui = dynamicScript->dynamicDefinition.renderGuiFunc;
                return renderGui(ptr.context, &state, ent, displaySize, scale, deltaTime);
            }
        }
        return nullptr;
    }

    DynamicScript::DynamicScript(DynamicLibrary &library, const DynamicScriptDefinition &dynamicDefinition)
        : ScriptDefinitionBase(this->metadata), metadata(typeid(void),
                                                    dynamicDefinition.contextSize,
                                                    dynamicDefinition.name.c_str(),
                                                    dynamicDefinition.desc ? dynamicDefinition.desc : ""),
          library(&library), dynamicDefinition(dynamicDefinition) {
        definition.name = dynamicDefinition.name;
        definition.type = dynamicDefinition.type;
        definition.events = dynamicDefinition.events;
        definition.filterOnEvent = dynamicDefinition.filterOnEvent;
        metadata.fields = dynamicDefinition.fields;
        switch (definition.type) {
        case ScriptType::LogicScript:
            definition.initFunc = ScriptInitFunc(&Init);
            definition.destroyFunc = ScriptDestroyFunc(&Destroy);
            definition.callback = LogicTickFunc(&OnTick);
            break;
        case ScriptType::PhysicsScript:
            definition.initFunc = ScriptInitFunc(&Init);
            definition.destroyFunc = ScriptDestroyFunc(&Destroy);
            definition.callback = PhysicsTickFunc(&OnTick);
            break;
        case ScriptType::EventScript:
            definition.initFunc = ScriptInitFunc(&Init);
            definition.destroyFunc = ScriptDestroyFunc(&Destroy);
            definition.callback = OnEventFunc(&OnEvent);
            break;
        case ScriptType::PrefabScript:
            definition.callback = PrefabFunc(&Prefab);
            break;
        case ScriptType::GuiScript:
            definition.initFunc = ScriptInitFunc(&Init);
            definition.destroyFunc = ScriptDestroyFunc(&Destroy);
            definition.callback = GuiRenderFuncs(&BeforeFrame, &RenderGui);
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
        case ScriptType::GuiScript:
            if (!definition.renderGuiFunc) {
                Errorf("Failed to load %s(%s), %s is missing sp_script_render_gui()",
                    library.name,
                    definition.name,
                    definition.type);
                return nullptr;
            }
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
