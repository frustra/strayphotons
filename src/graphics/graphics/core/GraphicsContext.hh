/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "console/CVar.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace ecs {
    struct View;
}

struct GLFWwindow;

namespace sp::winit {
    struct WinitContext;
}

namespace sp::vulkan {
    class DeviceContext;
    class PerfTimer;
} // namespace sp::vulkan

namespace sp {
    class GpuTexture;
    class Image;
    class DebugGuiManager;
    class MenuGuiManager;
    class Game;

    extern CVar<float> CVarFieldOfView;
    extern CVar<glm::ivec2> CVarWindowSize;
    extern CVar<bool> CVarWindowFullscreen;

    class GraphicsContext {
    public:
        GraphicsContext() {}
        virtual ~GraphicsContext() {}

        virtual void BeginFrame() = 0;
        virtual void SwapBuffers() = 0;
        virtual void EndFrame() = 0;
        virtual void WaitIdle() {}

        void AttachView(ecs::Entity e) {
            activeView = e;
        }

        const ecs::Entity &GetActiveView() const {
            return activeView;
        }

        virtual void InitRenderer(Game &game) = 0;
        virtual void RenderFrame(chrono_clock::duration elapsedTime) = 0;

        virtual void SetDebugGui(DebugGuiManager *debugGui) = 0;
        virtual void SetMenuGui(MenuGuiManager *menuGui) = 0;

        virtual const std::vector<glm::ivec2> &MonitorModes() {
            return monitorModes;
        }

        virtual std::shared_ptr<GpuTexture> LoadTexture(std::shared_ptr<const Image> image, bool genMipmap = true) = 0;

        // Returns the window HWND, if it exists. On non-Windows platforms this returns nullptr.
        virtual void *Win32WindowHandle() {
            return nullptr;
        }

        virtual uint32_t GetMeasuredFPS() const {
            return 0;
        }

    protected:
        ecs::Entity activeView;
        std::vector<glm::ivec2> monitorModes;
    };
} // namespace sp
