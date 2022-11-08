#pragma once

#include "console/CFunc.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/Events.hh"

namespace sp {

    class EditorSystem {
    public:
        EditorSystem();

        void OpenEditor(std::string targetName, bool flatMode = true);

    private:
        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");

        ecs::EventQueueRef events = ecs::NewEventQueue();

        CFuncCollection funcs;
    };

} // namespace sp
