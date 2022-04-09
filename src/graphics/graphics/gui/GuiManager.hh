#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

#include <glm/glm.hpp>
#include <memory>
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
        GuiManager(const std::string &name, ecs::FocusLayer layer = ecs::FocusLayer::GAME);
        virtual ~GuiManager();
        void Attach(const std::shared_ptr<GuiRenderable> &component);
        void SetGuiContext();

        virtual void BeforeFrame();
        virtual void DefineWindows();

        const std::string &Name() const {
            return name;
        }

    protected:
        ecs::EntityRef guiEntity, keyboardEntity, playerEntity;

        std::string name;
        ecs::FocusLayer focusLayer;

    private:
        std::vector<std::shared_ptr<GuiRenderable>> components;
        ImGuiContext *imCtx = nullptr;
    };
} // namespace sp
