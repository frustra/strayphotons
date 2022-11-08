#pragma once

#include "GuiContext.hh"

namespace sp {

    class SystemGuiManager : public GuiContext {
    public:
        SystemGuiManager(const std::string &name, ecs::FocusLayer layer = ecs::FocusLayer::Game);

        virtual void BeforeFrame();

    protected:
        ecs::EntityRef guiEntity;
        ecs::EntityRef keyboardEntity = ecs::Name("input", "keyboard");

        ecs::EventQueueRef events = ecs::NewEventQueue();

        ecs::FocusLayer focusLayer;
    };
} // namespace sp
