#pragma once

#include "ecs/NamedEntity.hh"

#include <glm/glm.hpp>
#include <vector>

struct ImGuiContext;

namespace sp {

    class GraphicsManager;

    class GuiRenderable {
    public:
        virtual void Add() = 0;
    };

    class GuiManager {
    public:
        GuiManager(GraphicsManager &graphics);
        virtual ~GuiManager();
        void Attach(GuiRenderable *component);
        void SetGuiContext();

        virtual void BeforeFrame();
        virtual void DefineWindows();

    protected:
        GraphicsManager &graphics;

        ecs::NamedEntity playerEntity;
        ecs::NamedEntity keyboardEntity;

    private:
        std::vector<GuiRenderable *> components;
        ImGuiContext *imCtx = nullptr;
    };
} // namespace sp
