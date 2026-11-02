/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "DynamicLibrary.hh"

#include "ecs/ScriptDefinition.hh"
#include "graphics/GenericCompositor.hh"
#include "strayphotons/HeapVector.hh"
#include "strayphotons/gui/GuiInputEvents.hh"

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
        auto *ptr = state.Get<DynamicScriptContext>();
        if (!ptr) {
            auto contextPtr = std::dynamic_pointer_cast<DynamicScript>(state.definition.context.lock());
            return *state.Set<DynamicScriptContext>(contextPtr);
        }
        return *ptr;
    }

    const void *DynamicScript::GetDefault() const {
        return defaultContext.context;
    }

    const void *DynamicScript::Access(const ScriptState &state) const {
        const auto *ptr = state.Get<DynamicScriptContext>();
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
                dynamicScript->dynamicDefinition.initFunc(ptr.context, state);
            }
        }
    }

    void DynamicScript::Destroy(ScriptState &state) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            auto *ptr = state.Get<DynamicScriptContext>();
            if (ptr && dynamicScript->dynamicDefinition.destroyFunc) {
                dynamicScript->dynamicDefinition.destroyFunc(ptr->context, state);
            }
        }
    }

    void DynamicScript::OnLogicTick(ScriptState &state,
        const LogicUpdateLock &lock,
        Entity ent,
        chrono_clock::duration interval) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            auto &ptr = dynamicScript->MaybeAllocContext(state);
            if (dynamicScript->dynamicDefinition.logicTickFunc) {
                DynamicLock<> dynLock = lock;
                dynamicScript->dynamicDefinition.logicTickFunc(ptr.context,
                    state,
                    dynLock,
                    ent,
                    std::chrono::nanoseconds(interval).count());
            }
        }
    }

    void DynamicScript::OnPhysicsTick(ScriptState &state,
        const PhysicsUpdateLock &lock,
        Entity ent,
        chrono_clock::duration interval) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            auto &ptr = dynamicScript->MaybeAllocContext(state);
            if (dynamicScript->dynamicDefinition.physicsTickFunc) {
                DynamicLock<> dynLock = lock;
                dynamicScript->dynamicDefinition.physicsTickFunc(ptr.context,
                    state,
                    dynLock,
                    ent,
                    std::chrono::nanoseconds(interval).count());
            }
        }
    }

    void DynamicScript::OnEvent(ScriptState &state, const DynamicLock<SendEventsLock> &lock, Entity ent, Event event) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            auto &ptr = dynamicScript->MaybeAllocContext(state);
            if (dynamicScript->dynamicDefinition.onEventFunc) {
                DynamicLock<> dynLock = lock;
                dynamicScript->dynamicDefinition.onEventFunc(ptr.context, state, dynLock, ent, event);
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
                dynamicScript->dynamicDefinition.prefabFunc(state, dynLock, ent, scene);
            }
        }
    }

    bool DynamicScript::BeforeFrame(sp::GenericCompositor &compositor,
        ScriptState &state,
        Entity ent,
        const sp::HeapVector<sp::GuiInputEvent> &inputEvents) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            auto &ptr = dynamicScript->MaybeAllocContext(state);
            if (dynamicScript->dynamicDefinition.beforeFrameFunc) {
                return dynamicScript->dynamicDefinition.beforeFrameFunc(ptr.context,
                    compositor,
                    state,
                    ent,
                    inputEvents);
            }
        }
        return false;
    }

    void DynamicScript::RenderGui(sp::GenericCompositor &compositor,
        ScriptState &state,
        Entity ent,
        glm::vec2 displaySize,
        glm::vec2 scale,
        float deltaTime,
        sp::GuiDrawData &result) {
        ZoneScoped;
        auto ctx = state.definition.context.lock();
        if (const auto *dynamicScript = dynamic_cast<const DynamicScript *>(ctx.get())) {
            ZoneStr(dynamicScript->definition.name);
            auto &ptr = dynamicScript->MaybeAllocContext(state);
            if (dynamicScript->dynamicDefinition.renderGuiFunc) {
                auto &renderGui = dynamicScript->dynamicDefinition.renderGuiFunc;
                renderGui(ptr.context, compositor, state, ent, displaySize, scale, deltaTime, result);
            }
        }
    }

#ifdef __clang__
// No warning on clang
#elif defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    DynamicScript::DynamicScript(DynamicLibrary &library, const DynamicScriptDefinition &dynamicDefinition)
        : ScriptDefinitionBase(this->metadata), metadata(typeid(void),
                                                    dynamicDefinition.contextSize,
                                                    dynamicDefinition.name.c_str(),
                                                    dynamicDefinition.desc ? dynamicDefinition.desc : ""),
          library(&library), dynamicDefinition(dynamicDefinition) {
#ifdef __clang__
// No warning on clang
#elif defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif
        definition.name = dynamicDefinition.name;
        definition.events = dynamicDefinition.events;
        metadata.fields.assign(dynamicDefinition.fields.begin(), dynamicDefinition.fields.end());
        definition.initFunc = ScriptInitFunc(&Init);
        definition.destroyFunc = ScriptDestroyFunc(&Destroy);
        if (dynamicDefinition.logicTickFunc) {
            definition.logicTickFunc = {
                LogicTickFunc(&OnLogicTick),
                dynamicDefinition.logicReadPermissions,
                dynamicDefinition.logicWritePermissions,
            };
        }
        if (dynamicDefinition.physicsTickFunc) {
            definition.physicsTickFunc = {
                PhysicsTickFunc(&OnPhysicsTick),
                dynamicDefinition.physicsReadPermissions,
                dynamicDefinition.physicsWritePermissions,
            };
        }
        if (dynamicDefinition.onEventFunc) {
            definition.onEventFunc = {
                OnEventFunc(&OnEvent),
                dynamicDefinition.eventReadPermissions,
                dynamicDefinition.eventWritePermissions,
            };
        }
        if (dynamicDefinition.prefabFunc) definition.prefabFunc = PrefabFunc(&Prefab);
        if (dynamicDefinition.beforeFrameFunc) definition.beforeFrameFunc = BeforeFrameFunc(&BeforeFrame);
        if (dynamicDefinition.renderGuiFunc) definition.renderGuiFunc = RenderGuiFunc(&RenderGui);
    }

    std::shared_ptr<DynamicScript> DynamicScript::Load(DynamicLibrary &library,
        const DynamicScriptDefinition &definition) {
        ZoneScoped;
        ZoneStr(definition.name);

        if (!definition.initFunc && !definition.destroyFunc && !definition.logicTickFunc &&
            !definition.physicsTickFunc && !definition.onEventFunc && !definition.prefabFunc &&
            !definition.beforeFrameFunc && !definition.renderGuiFunc) {
            Errorf("Failed to load %s(%s), script is missing definitions", library.name, definition.name);
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

    void DynamicScriptDefinition::AddEvent(const char *eventName) {
        events.emplace_back(eventName);
    }

    StructField &DynamicScriptDefinition::AddField(const char *name,
        uint32_t typeIndex,
        uint64_t size,
        uint64_t offset) {
        StructField &field = fields.emplace_back();
        field.name = name;
        field.type.typeIndex = typeIndex;
        field.size = size;
        field.offset = offset;
        return field;
    }

} // namespace ecs
