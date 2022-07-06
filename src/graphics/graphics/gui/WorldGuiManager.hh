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
            ecs::Entity sourceEntity;
            glm::vec2 mousePos;
        };

        vector<PointingState> pointingStack;
    };
} // namespace sp
