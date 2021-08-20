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
        // TODO: Fix focus
        GuiManager(GraphicsManager &graphics /*, const FocusLevel focusPriority = FOCUS_GAME*/);
        virtual ~GuiManager();
        void Attach(GuiRenderable *component);
        void SetGuiContext();

        virtual void BeforeFrame();
        virtual void DefineWindows();

    protected:
        // const FocusLevel focusPriority;
        GraphicsManager &graphics;

        ecs::NamedEntity playerEntity;
        ecs::NamedEntity keyboardEntity;

    private:
        std::vector<GuiRenderable *> components;
        ImGuiContext *imCtx = nullptr;
    };
} // namespace sp
