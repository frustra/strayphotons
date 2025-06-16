/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/ScriptImpl.hh"
#include "game/SceneRef.hh"

#include <dynalo/dynalo.hpp>
#include <memory>

namespace ecs {
    class DynamicScript;

    class DynamicScriptContext {
    public:
        DynamicScriptContext() : context(nullptr), script(nullptr) {}
        DynamicScriptContext(const DynamicScript &script);
        DynamicScriptContext(const DynamicScriptContext &other);
        // DynamicScriptContext(DynamicScriptContext &&other) = default;
        ~DynamicScriptContext();

        DynamicScriptContext &operator=(const DynamicScriptContext &other);
        // DynamicScriptContext &operator=(DynamicScriptContext &&other) = default;

        void *context;

    private:
        const DynamicScript *script;
    };

    class DynamicScript final : public ScriptDefinitionBase, sp::NonMoveable {
    public:
        const std::string name;
        StructMetadata metadata;
        ScriptDefinition definition;

        static std::shared_ptr<DynamicScript> Load(const std::string &name);
        DynamicScript(const std::string &name, dynalo::library &&lib, const ScriptDefinition &definition);

        void Register() const;
        void Reload();

        const void *GetDefault() const override;
        const void *Access(const ScriptState &state) const override;
        void *AccessMut(ScriptState &state) const override;

    private:
        std::optional<dynalo::library> dynamicLib;
        DynamicScriptContext defaultContext;

        void *(*newContextFunc)(const void *) = nullptr;
        void (*freeContextFunc)(void *) = nullptr;
        void (*initFunc)(void *, ScriptState *) = nullptr;
        void (*destroyFunc)(void *, ScriptState *) = nullptr;
        void (*onTickFunc)(void *, ScriptState *, void *, uint64_t, uint64_t) = nullptr;
        void (*onEventFunc)(void *, ScriptState *, void *, uint64_t, Event *) = nullptr;
        void (*prefabFunc)(const ScriptState *, void *, uint64_t, const sp::SceneRef *) = nullptr;

        template<typename Fn>
        void LoadFunc(Fn &ptr, const char *name) {
            ptr = dynamicLib->get_function<std::remove_pointer_t<Fn>>(name);
        }

        DynamicScriptContext &MaybeAllocContext(ScriptState &state) const;

        static void Init(ScriptState &state);
        static void Destroy(ScriptState &state);

        static void OnTick(ScriptState &state,
            const DynamicLock<ReadSignalsLock> &lock,
            Entity ent,
            chrono_clock::duration interval);
        static void OnEvent(ScriptState &state, const DynamicLock<ReadSignalsLock> &lock, Entity ent, Event event);
        static void Prefab(const ScriptState &state, const sp::SceneRef &scene, Lock<AddRemove> lock, Entity ent);

        friend class DynamicScriptContext;
    };
} // namespace ecs
