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

    protected:
        Model *model;
    };
}; // namespace sp
