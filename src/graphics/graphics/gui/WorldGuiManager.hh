#pragma once

#include "GuiContext.hh"
#include "ecs/Ecs.hh"

namespace sp {

    class WorldGuiManager : public GuiContext {
    public:
        WorldGuiManager(ecs::Entity gui, const std::string &name);

        void RegisterEvents(ecs::Lock<ecs::Write<ecs::EventInput>> lock);

        virtual void BeforeFrame();
        void DefineWindows() override;

    private:
        ecs::EntityRef guiEntity;

        ecs::EventQueueRef events = ecs::NewEventQueue();

        struct PointingState {
            ecs::Entity sourceEntity;
            glm::vec2 mousePos;
        };

        vector<PointingState> pointingStack;
    };
} // namespace sp
