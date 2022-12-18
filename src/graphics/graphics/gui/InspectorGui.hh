#pragma once

#include "graphics/gui/GuiContext.hh"

#include <memory>
#include <string>

namespace sp {
    struct EditorContext;

    class InspectorGui : public GuiWindow {
    public:
        InspectorGui(const std::string &name);
        virtual ~InspectorGui() {}

        void DefineContents();

    private:
        std::shared_ptr<EditorContext> context;
    };
} // namespace sp
