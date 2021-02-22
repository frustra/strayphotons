#pragma once

#include "Texture.hh"
#include "VertexBuffer.hh"
#include "ecs/components/View.hh"

namespace sp {
    class VoxelRenderer;
    class GuiManager;

    class GuiRenderer {
    public:
        GuiRenderer(VoxelRenderer &renderer, GuiManager *manager);
        ~GuiRenderer();
        void Render(ecs::View view);

    private:
        VertexBuffer vertices, indices;
        Texture fontTex;
        double lastTime = 0.0;

        VoxelRenderer &parent;
        GuiManager *manager;
    };
} // namespace sp
