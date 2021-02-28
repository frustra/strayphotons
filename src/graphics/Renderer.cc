#include "graphics/Renderer.hh"
#include "graphics/SceneShaders.hh"
namespace sp {
    Renderer::Renderer(Game* g) : game(g) {
        GlobalShaders = new ShaderSet();
    }

    Renderer::~Renderer() {
        delete GlobalShaders;
    }
};

