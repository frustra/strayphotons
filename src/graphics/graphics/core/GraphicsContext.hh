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

namespace sp {
    class Game;
    class GenericCompositor;
    class GpuTexture;
    class GuiContext;
    class Image;

    extern CVar<float> CVarFieldOfView;
    extern CVar<glm::ivec2> CVarWindowSize;
    extern CVar<glm::vec2> CVarWindowScale;
    extern CVar<bool> CVarWindowFullscreen;

    class GraphicsContext {
    public:
        GraphicsContext() {}
        virtual ~GraphicsContext() {}

        // Skips frame if false is returned
        virtual bool BeginFrame() = 0;
        virtual void SwapBuffers() = 0;
        virtual void EndFrame() = 0;
        virtual void WaitIdle() {}
        virtual bool RequiresReset() const {
            return false;
        }

        virtual void AttachWindow(const std::shared_ptr<GuiContext> &context) = 0;

        virtual void InitRenderer(Game &game) = 0;

        virtual GenericCompositor &GetCompositor() = 0;

        virtual void RenderFrame(chrono_clock::duration elapsedTime) = 0;

        virtual const std::vector<glm::ivec2> &MonitorModes() {
            return monitorModes;
        }

        // Returns the window HWND, if it exists. On non-Windows platforms this returns nullptr.
        virtual void *Win32WindowHandle() {
            return nullptr;
        }

        virtual uint32_t GetMeasuredFPS() const {
            return 0;
        }

    protected:
        std::vector<glm::ivec2> monitorModes;
    };
} // namespace sp
