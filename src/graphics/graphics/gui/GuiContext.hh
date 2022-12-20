#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <vector>

struct ImGuiContext;

namespace sp {
    enum class Font {
        Primary,
        Accent,
        Monospace,
    };

    struct FontDef {
        Font type;
        const char *name;
        float size;
    };

    class GuiRenderable {
    public:
        GuiRenderable(const string &name) : name(name) {}

        const string name;
        virtual void DefineContents() = 0;

        void PushFont(Font fontType, float fontSize);
    };

    class GuiWindow : public GuiRenderable {
    public:
        GuiWindow(const string &name, int flags = 0) : GuiRenderable(name), flags(flags) {}

        virtual void PreDefine() {}

        int flags = 0;
    };

    class GuiContext {
    public:
        GuiContext(const std::string &name);
        virtual ~GuiContext();
        void Attach(const std::shared_ptr<GuiRenderable> &component);
        void Detach(const std::shared_ptr<GuiRenderable> &component);
        void SetGuiContext();

        virtual void BeforeFrame();
        virtual void DefineWindows() = 0;

        const std::string &Name() const {
            return name;
        }

        void PushFont(Font fontType, float fontSize);

    protected:
        std::vector<std::shared_ptr<GuiRenderable>> components;

    private:
        ImGuiContext *imCtx = nullptr;
        std::string name;
    };

    shared_ptr<GuiWindow> CreateGuiWindow(const string &name);

    std::span<FontDef> GetFontList();
} // namespace sp
