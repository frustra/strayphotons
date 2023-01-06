#pragma once

#include "ecs/EntityRef.hh"
#include "graphics/gui/GuiContext.hh"

#include <memory>
#include <string>

namespace sp {
    struct EditorContext;
    class Scene;

    class InspectorGui : public GuiWindow {
    public:
        InspectorGui(const std::string &name);
        virtual ~InspectorGui() {}

        void PreDefine() override;
        void DefineContents() override;
        void PostDefine() override;

    private:
        ecs::EventQueueRef events = ecs::NewEventQueue();
        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");
        ecs::EntityRef targetEntity;
        std::shared_ptr<Scene> targetScene;

        std::shared_ptr<EditorContext> context;
    };
} // namespace sp
