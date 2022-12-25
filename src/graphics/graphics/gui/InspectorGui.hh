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

        void PreDefine() override;
        void DefineContents() override;
        void PostDefine() override;

    private:
        std::shared_ptr<EditorContext> context;
    };
} // namespace sp
