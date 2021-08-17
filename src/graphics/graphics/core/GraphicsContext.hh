#pragma once

#include "core/CVar.hh"
#include "graphics/core/RenderTarget.hh"

#include <Tecs.hh>
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

    extern CVar<glm::ivec2> CVarWindowSize;
    extern CVar<float> CVarWindowScale;
    extern CVar<float> CVarFieldOfView;
    extern CVar<int> CVarWindowFullscreen;

    class GraphicsContext {
    public:
        GraphicsContext() {}
        virtual ~GraphicsContext() {}

        virtual bool ShouldClose() = 0;
        virtual void BeginFrame() = 0;
        virtual void SwapBuffers() = 0;
        virtual void EndFrame() = 0;

        virtual void DisableCursor() {}
        virtual void EnableCursor() {}

        void AttachView(Tecs::Entity e) {
            activeView = e;
        }

        const Tecs::Entity &GetActiveView() const {
            return activeView;
        }

        virtual void PrepareWindowView(ecs::View &view) = 0;

        // These functions are acceptable in the base GraphicsContext class,
        // but really shouldn't needed. They should be replaced with a generic "Settings" API
        // that allows modules to populate a Settings / Options menu entry
        //
        // TODO: default implementation for these functions for graphics contexts that don't support
        // monitor modes
        virtual const std::vector<glm::ivec2> &MonitorModes() = 0;
        virtual const glm::ivec2 CurrentMode() = 0;

        virtual std::shared_ptr<GpuTexture> LoadTexture(std::shared_ptr<Image> image, bool genMipmap = true) = 0;

        // Returns the window HWND, if it exists. On non-Windows platforms this returns nullptr.
        virtual void *Win32WindowHandle() {
            return nullptr;
        }

    protected:
        Tecs::Entity activeView;
    };
} // namespace sp
