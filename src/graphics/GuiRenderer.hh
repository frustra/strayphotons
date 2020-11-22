#pragma once

#include "Texture.hh"
#include "VertexBuffer.hh"
#include "ecs/components/View.hh"

namespace sp {
    class Renderer;
    class GuiManager;

    class GuiRenderer {
    public:
        GuiRenderer(Renderer &renderer, GuiManager *manager);
        ~GuiRenderer();
        void Render(ecs::View view);

    private:
        VertexBuffer vertices, indices;
        Texture fontTex;
        double lastTime = 0.0;

        Renderer &parent;
        GuiManager *manager;
    };
} // namespace sp
