#pragma once

#include "console/CVar.hh"
#include "ecs/Ecs.hh"
#include "graphics/core/RenderTarget.hh"

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace ecs {
    struct View;
}

namespace sp {
    class GpuTexture;
    class Image;

    extern CVar<float> CVarFieldOfView;
    extern CVar<glm::ivec2> CVarWindowSize;
    extern CVar<bool> CVarWindowFullscreen;

    class GraphicsContext {
    public:
        GraphicsContext() {}
        virtual ~GraphicsContext() {}

        virtual bool ShouldClose() = 0;
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

        virtual void PrepareWindowView(ecs::View &view) = 0;
        virtual void UpdateInputModeFromFocus() = 0;

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
        virtual void SetTitle(std::string title) {}

    protected:
        ecs::Entity activeView;
        std::vector<glm::ivec2> monitorModes;
    };
} // namespace sp
