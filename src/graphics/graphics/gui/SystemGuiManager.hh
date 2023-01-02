#pragma once

#include "GuiContext.hh"

namespace sp {

    class SystemGuiManager : public GuiContext {
    public:
        SystemGuiManager(const std::string &name);

        virtual void BeforeFrame();

    protected:
        ecs::EntityRef guiEntity;
        ecs::EventQueueRef events = ecs::NewEventQueue();
    };
} // namespace sp
