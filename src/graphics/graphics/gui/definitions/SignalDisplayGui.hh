#pragma once

#include "ecs/EntityRef.hh"
#include "graphics/gui/GuiContext.hh"

namespace sp {
    class SignalDisplayGui : public GuiWindow {
    public:
        SignalDisplayGui(const std::string &name, const ecs::Entity &ent);
        virtual ~SignalDisplayGui() {}

        void PreDefine() override;
        void DefineContents() override;
        void PostDefine() override;

    private:
        ecs::EntityRef signalEntity;
    };
} // namespace sp
