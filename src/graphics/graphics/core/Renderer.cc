#include "Renderer.hh"

#include "graphics/opengl/SceneShaders.hh"

namespace sp {
    Renderer::Renderer(ecs::EntityManager &ecs) : ecs(ecs) {
        GlobalShaders = new ShaderSet();
    }

    Renderer::~Renderer() {
        delete GlobalShaders;
    }
}; // namespace sp
