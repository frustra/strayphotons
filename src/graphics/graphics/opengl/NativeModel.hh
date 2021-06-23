#pragma once

#include "assets/Model.hh"
#include "ecs/components/View.hh"
#include "graphics/opengl/Renderer.hh"

#include <glm/glm.hpp>

namespace sp {
    class SceneShader;

    /**
     * @brief Interface of a "native" model, that can be used by a Renderer
     *        to draw on screen.
     *
     *        This class must never depend on graphics-backend-specific types (like GLuint)
     *        so that it can be compiled into the headless / core engine.
     */
    class NativeModel {
    public:
        NativeModel(Model *m, Renderer *r) : model(m), renderer(r) {}
        ~NativeModel() = default;

        virtual void Draw(SceneShader *shader,
                          glm::mat4 modelMat,
                          const ecs::View &view,
                          int boneCount,
                          glm::mat4 *boneData) = 0;

    protected:
        Model *model;
        Renderer *renderer;
    };
}; // namespace sp
