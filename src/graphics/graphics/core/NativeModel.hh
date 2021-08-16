#pragma once

#include "assets/Model.hh"
#include "ecs/components/View.hh"

#include <glm/glm.hpp>

namespace sp {
    class SceneShader;

    class NativeModel {
    public:
        NativeModel(Model *m) : model(m) {}
        ~NativeModel() = default;

        virtual void Draw(SceneShader *shader,
                          glm::mat4 modelMat,
                          const ecs::View &view,
                          int boneCount,
                          glm::mat4 *boneData) = 0;

    protected:
        Model *model;
    };
}; // namespace sp
