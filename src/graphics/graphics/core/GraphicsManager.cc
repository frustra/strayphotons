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
#include "graphics/gui/MenuGuiManager.hh"
#include "graphics/gui/OverlayGuiManager.hh"

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

    static CVar<uint32> CVarMaxFPS("r.MaxFPS", 144, "wait between frames to target this framerate (0 to disable)");

    GraphicsManager::GraphicsManager(Game &game)
        : RegisteredThread("RenderThread", CVarMaxFPS.Get(), true), game(game) {
        if (game.options.count("window-size")) {
            std::istringstream ss(game.options["window-size"].as<string>());
            glm::ivec2 size = glm::ivec2(0);
            ss >> size.x >> size.y;

            if (size.x > 0 && size.y > 0) {
                CVarWindowSize.Set(size);
            }
        }
        if (game.options.count("window-scale")) {
            std::istringstream ss(game.options["window-scale"].as<string>());
            glm::vec2 windowScale = glm::vec2(0);
            ss >> windowScale;
            if (windowScale.x > 0.0f) {
                if (windowScale.y <= 0.0f) windowScale.y = windowScale.x;
                CVarWindowScale.Set(windowScale);
            }
        }
    }

    GraphicsManager::~GraphicsManager() {
        StopThread();
        if (context) context->WaitIdle();
    }

    void GraphicsManager::Init() {
        ZoneScoped;
        Assert(!initialized, "GraphicsManager initialized twice");
        initialized = true;

        overlayGui = std::make_shared<OverlayGuiManager>();
        menuGui = std::make_shared<MenuGuiManager>(*this);
    }

    void GraphicsManager::StartThread(bool startPaused) {
        RegisteredThread::StartThread(startPaused);
    }

    void GraphicsManager::StopThread() {
        RegisteredThread::StopThread();
    }

    bool GraphicsManager::HasActiveContext() {
        bool shouldClose = windowHandlers.should_close && windowHandlers.should_close(this);
        return context && !shouldClose && state == ThreadState::Started;
    }

    bool GraphicsManager::ThreadInit() {
        ZoneScoped;
        renderStart = chrono_clock::now();

        Assert(context, "Invalid vulkan context on init");

        context->InitRenderer(game);
        context->SetOverlayGui(overlayGui.get());
        context->SetMenuGui(menuGui.get());

        return true;
    }

    bool GraphicsManager::InputFrame() {
        ZoneScoped;
        if (!HasActiveContext()) return false;

        if (!flatviewName || CVarFlatviewEntity.Changed()) {
            flatviewName = ecs::Name(CVarFlatviewEntity.Get(true), ecs::Name());
        }

        if (windowHandlers.set_title) {
            std::string newTitle = "STRAY PHOTONS (" + std::to_string(context->GetMeasuredFPS()) + " FPS)";
            windowHandlers.set_title(this, newTitle.c_str());
        }
        if (windowHandlers.set_cursor_visible) {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::FocusLock>>();
            if (lock.Has<ecs::FocusLock>()) {
                auto layer = lock.Get<ecs::FocusLock>().PrimaryFocus();
                if (layer == ecs::FocusLayer::Game) {
                    windowHandlers.set_cursor_visible(this, false);
                } else {
                    windowHandlers.set_cursor_visible(this, true);
                }
            }
        }

        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::View>>();

            for (auto &ent : lock.EntitiesWith<ecs::View>()) {
                if (!ent.Has<ecs::Name>(lock)) continue;
                auto &name = ent.Get<ecs::Name>(lock);
                if (name == flatviewName) {
                    auto &view = ent.Get<ecs::View>(lock);
                    if (windowHandlers.update_window_view) {
                        windowHandlers.update_window_view(this, &view.extents.x, &view.extents.y);
                    }

                    if (view.extents == glm::ivec2(0)) view.extents = CVarWindowSize.Get();
                    view.fov = glm::radians(CVarFieldOfView.Get());
                    view.UpdateProjectionMatrix();

                    context->AttachView(ent);
                }
            }
        }
        return true;
    }

    bool GraphicsManager::PreFrame() {
        ZoneScoped;
        if (!HasActiveContext()) return false;
        if (overlayGui) overlayGui->BeforeFrame();
        if (menuGui) menuGui->BeforeFrame();

        return context->BeginFrame();
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
