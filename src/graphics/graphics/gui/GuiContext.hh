#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

struct ImGuiContext;

namespace sp {
    class GuiRenderable {
    public:
        virtual void Add() = 0;
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

    private:
        std::vector<std::shared_ptr<GuiRenderable>> components;
        ImGuiContext *imCtx = nullptr;
        std::string name;
    };

    shared_ptr<GuiContext> CreateGuiWindow(const string &name);
} // namespace sp
