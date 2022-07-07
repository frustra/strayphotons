#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

#include <glm/glm.hpp>
#include <memory>
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
        virtual void Add() = 0;

        void PushFont(Font fontType, float fontSize);
    };

    class GuiWindow : public GuiRenderable {
    public:
        const string name;

        GuiWindow(const string &name) : name(name) {}
        virtual void DefineContents() = 0;

        virtual void Add();
    };

    class GuiContext {
    public:
        GuiContext(const std::string &name);
        virtual ~GuiContext();
        void Attach(const std::shared_ptr<GuiRenderable> &component);
        void SetGuiContext();

        virtual void BeforeFrame();
        virtual void DefineWindows();

        const std::string &Name() const {
            return name;
        }

        void PushFont(Font fontType, float fontSize);

    private:
        std::vector<std::shared_ptr<GuiRenderable>> components;
        ImGuiContext *imCtx = nullptr;
        std::string name;
    };

    bool CreateGuiWindow(GuiContext *context, const string &name);

    std::span<FontDef> GetFontList();
} // namespace sp
