#pragma once

#include "console/CFunc.hh"
#include "ecs/Ecs.hh"

namespace sp {
    class EditorSystem {
    public:
        EditorSystem();
        ~EditorSystem();

        void OpenEditor(std::string targetName);

    private:
        ecs::EntityRef targetEntity;
        ecs::Entity previousTargetEntity = {};
        ecs::EntityRef playerEntity = ecs::Name("player", "player");
        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");

        CFuncCollection funcs;
    };

} // namespace sp
