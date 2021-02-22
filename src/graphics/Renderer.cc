#include "graphics/Renderer.hh"

namespace sp {
    Renderer::Renderer(Game* g) : game(g) {
        GlobalShaders = new ShaderSet();
    }

    Renderer::~Renderer() {
        delete GlobalShaders;
    }
};

