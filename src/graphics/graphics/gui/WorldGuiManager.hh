#pragma once

#include "GuiContext.hh"

namespace sp {

    class WorldGuiManager : public GuiContext {
    public:
        WorldGuiManager(ecs::Entity guiEntity, const std::string &name);

        virtual void BeforeFrame();

    private:
        ecs::EntityRef guiEntity;

        struct PointingState {
            glm::vec2 mousePos;
            chrono_clock::time_point time;

            PointingState() : time(chrono_clock::now()) {}
        };

        std::map<ecs::Entity, PointingState> pointers;
    };
} // namespace sp
