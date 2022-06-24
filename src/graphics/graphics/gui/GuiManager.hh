#pragma once

#include "GuiContext.hh"

namespace sp {

    class GuiManager : public GuiContext {
    public:
        GuiManager(const std::string &name, ecs::FocusLayer layer = ecs::FocusLayer::GAME);

        virtual void BeforeFrame();

    protected:
        ecs::EntityRef guiEntity;
        ecs::EntityRef keyboardEntity = ecs::Name("input", "keyboard");
        ecs::EntityRef playerEntity = ecs::Name("player", "player");

        ecs::FocusLayer focusLayer;
    };
} // namespace sp
