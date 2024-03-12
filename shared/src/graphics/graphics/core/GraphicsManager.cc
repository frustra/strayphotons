/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GraphicsManager.hh"

#include "common/Logging.hh"
#include "common/Tracing.hh"
#include "console/CVar.hh"
#include "ecs/EcsImpl.hh"
#include "game/CGameContext.hh"
#include "graphics/core/GraphicsContext.hh"
#include "graphics/gui/DebugGuiManager.hh"
#include "graphics/gui/MenuGuiManager.hh"

#include <algorithm>
#include <cxxopts.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <iostream>
#include <optional>
#include <system_error>
#include <thread>

namespace sp {
    static CVar<std::string> CVarFlatviewEntity("r.FlatviewEntity",
        "player:flatview",
        "The entity with a View component to display");

#ifdef SP_PACKAGE_RELEASE
    const uint32 DefaultMaxFPS = 0;
#else
    const uint32 DefaultMaxFPS = 144;
#endif

    static CVar<uint32> CVarMaxFPS("r.MaxFPS",
        DefaultMaxFPS,
        "wait between frames to target this framerate (0 to disable)");

    GraphicsManager::GraphicsManager(Game &game) : RegisteredThread("RenderThread", DefaultMaxFPS, true), game(game) {}

    GraphicsManager::~GraphicsManager() {
        StopThread();
        if (context) context->WaitIdle();
    }

    void GraphicsManager::Init() {
        ZoneScoped;
        Assert(!initialized, "GraphicsManager initialized twice");
        initialized = true;

        if (game.options.count("size")) {
            std::istringstream ss(game.options["size"].as<string>());
            glm::ivec2 size;
            ss >> size.x >> size.y;

            if (size.x > 0 && size.y > 0) {
                CVarWindowSize.Set(size);
            }
        }

        debugGui = std::make_shared<DebugGuiManager>();
        menuGui = std::make_shared<MenuGuiManager>(*this);
    }

    void GraphicsManager::StartThread(bool startPaused) {
        RegisteredThread::StartThread(startPaused);
    }

    void GraphicsManager::StopThread() {
        RegisteredThread::StopThread();
    }

    bool GraphicsManager::HasActiveContext() {
        return context && !context->ShouldClose();
    }

    bool GraphicsManager::ThreadInit() {
        ZoneScoped;
        renderStart = chrono_clock::now();

        Assert(context, "Invalid vulkan context on init");

        context->InitRenderer(game);
        context->SetDebugGui(debugGui.get());
        context->SetMenuGui(menuGui.get());

        return true;
    }

    bool GraphicsManager::InputFrame() {
        ZoneScoped;
        if (!HasActiveContext()) return false;

        if (!flatviewName || CVarFlatviewEntity.Changed()) {
            flatviewName = ecs::Name(CVarFlatviewEntity.Get(true), ecs::Name());
        }

        context->SetTitle("STRAY PHOTONS (" + std::to_string(context->GetMeasuredFPS()) + " FPS)");
        context->UpdateInputModeFromFocus();

        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::View>>();

            for (auto &ent : lock.EntitiesWith<ecs::View>()) {
                if (!ent.Has<ecs::Name>(lock)) continue;
                auto &name = ent.Get<ecs::Name>(lock);
                if (name == flatviewName) {
                    auto &view = ent.Get<ecs::View>(lock);
                    context->PrepareWindowView(view);
                    context->AttachView(ent);
                }
            }
        }
        return true;
    }

    void GraphicsManager::PreFrame() {
        ZoneScoped;
        if (!HasActiveContext()) return;
        if (debugGui) debugGui->BeforeFrame();
        if (menuGui) menuGui->BeforeFrame();

        context->BeginFrame();
    }

    void GraphicsManager::Frame() {
        ZoneScoped;
        if (!HasActiveContext()) return;

        if (stepMode) {
            context->RenderFrame(interval * stepCount.load());
        } else {
            context->RenderFrame(chrono_clock::now() - renderStart);
        }
    }

    void GraphicsManager::PostFrame(bool stepMode) {
        auto maxFPS = CVarMaxFPS.Get();
        if (maxFPS > 0) {
            interval = std::chrono::nanoseconds((int64_t)(1e9 / maxFPS));
        } else {
            interval = std::chrono::nanoseconds(0);
        }

        if (!HasActiveContext()) return;

        context->SwapBuffers();

        FrameMark;
        context->EndFrame();

        if (stepMode) {
            // Wait for graphics queue to complete so GPU readback is deterministic
            context->WaitIdle();
        }
    }
} // namespace sp
