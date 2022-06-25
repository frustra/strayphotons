#pragma once

#include "GuiContext.hh"

namespace sp {

    class WorldGuiManager : public GuiContext {
    public:
        WorldGuiManager(ecs::Entity guiEntity, const std::string &name);

        virtual void BeforeFrame();

    private:
        ecs::EntityRef guiEntity;
    };
} // namespace sp
