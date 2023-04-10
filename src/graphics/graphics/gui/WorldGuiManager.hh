#pragma once

#include "GuiContext.hh"
#include "ecs/Ecs.hh"
#include "ecs/EventQueue.hh"

namespace sp {

    class WorldGuiManager : public GuiContext {
    public:
        WorldGuiManager(ecs::Entity gui, const std::string &name);

        void BeforeFrame() override;
        void DefineWindows() override;

    private:
        ecs::EntityRef guiEntity;

        ecs::EventQueueRef events = ecs::NewEventQueue();

        struct PointingState {
            ecs::Entity sourceEntity;
            glm::vec2 mousePos;
            bool mouseDown = false;
        };

        vector<PointingState> pointingStack;
    };
} // namespace sp
