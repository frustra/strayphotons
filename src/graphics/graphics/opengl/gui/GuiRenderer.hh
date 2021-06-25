#pragma once

#include "ecs/components/View.hh"
#include "graphics/opengl/GLTexture.hh"
#include "graphics/opengl/VertexBuffer.hh"

namespace sp {
    class VoxelRenderer;
    class GlfwGraphicsContext;
    class GuiManager;

    class GuiRenderer {
    public:
        GuiRenderer(VoxelRenderer &renderer, GlfwGraphicsContext &context, GuiManager *manager);
        ~GuiRenderer();
        void Render(ecs::View view);

    private:
        VertexBuffer vertices, indices;
        GLTexture fontTex;
        double lastTime = 0.0;

        VoxelRenderer &parent;
        GuiManager *manager;
    };
} // namespace sp
