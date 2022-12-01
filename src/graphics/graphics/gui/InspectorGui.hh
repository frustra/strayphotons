#pragma once

#include "ecs/Ecs.hh"
#include "graphics/gui/GuiContext.hh"

#include <string>
#include <vector>

namespace sp {
    class InspectorGui : public GuiWindow {
    public:
        InspectorGui(const std::string &name);
        virtual ~InspectorGui() {}

        void DefineContents();

    private:
        void ListEntitiesByTransformTree();
        void AppendEntity(int depth, ecs::Entity ent, ecs::Lock<ecs::Read<ecs::Name, ecs::TransformTree>> lock);

        std::vector<std::vector<ecs::Entity>> children;

        ecs::EventQueueRef events = ecs::NewEventQueue();

        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");
        ecs::EntityRef targetEntity;
    };
} // namespace sp
