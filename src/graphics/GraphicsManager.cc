/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifdef SP_GRAPHICS_SUPPORT
    #include "GraphicsManager.hh"

    #include "console/CVar.hh"
    #include "core/Logging.hh"
    #include "core/Tracing.hh"
    #include "ecs/EcsImpl.hh"
    #include "graphics/core/GraphicsContext.hh"
    #include "main/Game.hh"

    #ifdef SP_GRAPHICS_SUPPORT_VK
        #include "graphics/vulkan/Renderer.hh"
        #include "graphics/vulkan/core/DeviceContext.hh"
        #include "graphics/vulkan/gui/ProfilerGui.hh"
    #endif

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

    #if defined(SP_GRAPHICS_SUPPORT_HEADLESS) || defined(SP_TEST_MODE)
    const uint32 DefaultMaxFPS = 90;
    #elif defined(SP_DEBUG)
    const uint32 DefaultMaxFPS = 144;
    #else
    const uint32 DefaultMaxFPS = 0;
    #endif

    static CVar<uint32> CVarMaxFPS("r.MaxFPS",
        DefaultMaxFPS,
        "wait between frames to target this framerate (0 to disable)");

    GraphicsManager::GraphicsManager(Game *game, bool stepMode)
        : RegisteredThread("RenderThread", DefaultMaxFPS, true), game(game), stepMode(stepMode) {}

    GraphicsManager::~GraphicsManager() {
        StopThread();
        if (context) context->WaitIdle();
    }

    void GraphicsManager::Init() {
        ZoneScoped;
        Assert(!context, "already have a graphics context");

    #ifdef SP_GRAPHICS_SUPPORT_VK
        Logf("Graphics starting up (Vulkan)");

        bool enableValidationLayers = game->options.count("with-validation-layers") > 0;

        #ifdef SP_GRAPHICS_SUPPORT_HEADLESS
        bool enableSwapchain = false;
        #else
        bool enableSwapchain = true;
        #endif
        auto vkContext = new vulkan::DeviceContext(enableValidationLayers, enableSwapchain);
        context.reset(vkContext);

        #if defined(SP_GRAPHICS_SUPPORT_GLFW) && defined(SP_INPUT_SUPPORT_GLFW)
        GLFWwindow *window = vkContext->GetGlfwWindow();
        if (window != nullptr) glfwInputHandler = make_unique<GlfwInputHandler>(game->windowEventQueue, *window);
        #endif
        #if defined(SP_GRAPHICS_SUPPORT_WINIT) && defined(SP_INPUT_SUPPORT_WINIT)
        gfx::WinitContext *winitCtx = vkContext->GetWinitContext();
        if (winitCtx != nullptr) winitInputHandler = make_unique<WinitInputHandler>(game->windowEventQueue, *winitCtx);
        #endif
    #endif

        if (game->options.count("size")) {
            std::istringstream ss(game->options["size"].as<string>());
            glm::ivec2 size;
            ss >> size.x >> size.y;

            if (size.x > 0 && size.y > 0) {
                CVarWindowSize.Set(size);
            }
        }
    }

    void GraphicsManager::StartThread() {
        RegisteredThread::StartThread(stepMode);
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

    #ifdef SP_GRAPHICS_SUPPORT_VK
        Assert(!renderer, "already have an active renderer");
        auto *vkContext = dynamic_cast<vulkan::DeviceContext *>(context.get());
        Assert(vkContext, "Invalid vulkan context on init");

        game->debugGui->Attach(make_shared<vulkan::ProfilerGui>(*vkContext->GetPerfTimer()));

        renderer = make_unique<vulkan::Renderer>(*vkContext);
        renderer->SetDebugGui(game->debugGui.get());
        renderer->SetMenuGui(game->menuGui.get());
    #endif

        return true;
    }

    bool GraphicsManager::InputFrame() {
        ZoneScoped;
    #ifdef SP_GRAPHICS_SUPPORT_VK
        if (!HasActiveContext()) return false;
    #endif

    #ifdef SP_INPUT_SUPPORT_GLFW
        if (glfwInputHandler) glfwInputHandler->Frame();
    #endif
    #ifdef SP_INPUT_SUPPORT_WINIT
        if (winitInputHandler) winitInputHandler->Frame();
    #endif

        if (!flatviewEntity || CVarFlatviewEntity.Changed()) {
            ecs::Name flatviewName(CVarFlatviewEntity.Get(true), ecs::Name());
            if (flatviewName) flatviewEntity = flatviewName;
        }

        context->SetTitle("STRAY PHOTONS (" + std::to_string(context->GetMeasuredFPS()) + " FPS)");
        context->UpdateInputModeFromFocus();

        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::View>>();

            auto flatview = flatviewEntity.Get(lock);
            if (flatview.Has<ecs::View>(lock)) {
                auto &view = flatview.Get<ecs::View>(lock);
                context->PrepareWindowView(view);
            }
            context->AttachView(flatview);
        }
        return true;
    }

    void GraphicsManager::PreFrame() {
        ZoneScoped;
        if (!HasActiveContext()) return;
        if (game->debugGui) game->debugGui->BeforeFrame();
        if (game->menuGui) game->menuGui->BeforeFrame();

        context->BeginFrame();
    }

    void GraphicsManager::Frame() {
        ZoneScoped;
        if (!HasActiveContext()) return;

    #ifdef SP_XR_SUPPORT
        auto xrSystem = game->xr.GetXrSystem();
        if (xrSystem) xrSystem->WaitFrame();
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
        Assert(renderer, "missing renderer");

        #ifdef SP_XR_SUPPORT
        renderer->SetXRSystem(xrSystem);
        #endif

        #ifdef SP_TEST_MODE
        renderer->RenderFrame(interval * stepCount.load());
        #else
        renderer->RenderFrame(chrono_clock::now() - renderStart);
        #endif

        #ifdef SP_XR_SUPPORT
        renderer->SetXRSystem(nullptr);
        #endif
    #endif
    }

    void GraphicsManager::PostFrame() {
        auto maxFPS = CVarMaxFPS.Get();
        if (maxFPS > 0) {
            interval = std::chrono::nanoseconds((int64_t)(1e9 / maxFPS));
        } else {
            interval = std::chrono::nanoseconds(0);
        }

        if (!HasActiveContext()) return;

    #ifndef SP_GRAPHICS_SUPPORT_HEADLESS
        context->SwapBuffers();
    #endif

        FrameMark;
    #ifdef SP_GRAPHICS_SUPPORT_VK
        Assert(renderer, "missing renderer");
        renderer->EndFrame();
    #endif
        context->EndFrame();

        if (stepMode) {
            // Wait for graphics queue to complete so GPU readback is deterministic
            context->WaitIdle();
        }
    }

    GraphicsContext *GraphicsManager::GetContext() {
        return context.get();
    }
} // namespace sp

#endif
