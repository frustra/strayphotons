#pragma once

#include "input/InputManager.hh"

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
        GuiManager(GraphicsManager &graphics, const FocusLevel focusPriority = FOCUS_GAME);
        virtual ~GuiManager();
        void Attach(GuiRenderable *component);
        void SetGuiContext();

        virtual void BeforeFrame();
        virtual void DefineWindows();

    protected:
        const FocusLevel focusPriority;
        GraphicsManager &graphics;

    private:
        std::vector<GuiRenderable *> components;
        ImGuiContext *imCtx = nullptr;
    };
} // namespace sp
