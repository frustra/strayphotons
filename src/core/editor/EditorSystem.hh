#pragma once

#include "console/CFunc.hh"
#include "ecs/Ecs.hh"

namespace sp {

    class EditorSystem {
    public:
        EditorSystem();
        ~EditorSystem();

        void OpenEditorFlat(std::string targetName);
        void OpenEditorWorld(std::string targetName);

    private:
        void OpenEditor(std::string targetName, bool flatMode);

        ecs::EntityRef playerEntity = ecs::Name("player", "player");
        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");

        CFuncCollection funcs;
    };

} // namespace sp
