#pragma once

#include "graphics/Texture.hh"
#include "graphics/VertexBuffer.hh"
#include "ecs/components/View.hh"

namespace sp {
    class VoxelRenderer;
    class GlfwGraphicsContext;
    class GuiManager;

    class GuiRenderer {
    public:
        GuiRenderer(VoxelRenderer& renderer, GlfwGraphicsContext& context, GuiManager *manager);
        ~GuiRenderer();
        void Render(ecs::View view);

    private:
        VertexBuffer vertices, indices;
        Texture fontTex;
        double lastTime = 0.0;

        GlfwGraphicsContext& context;
        VoxelRenderer& parent;
        GuiManager* manager;
    };
} // namespace sp
