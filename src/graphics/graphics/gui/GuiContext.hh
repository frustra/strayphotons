/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/Gui.hh"

#include <memory>
#include <string>
#include <vector>

struct ImGuiContext;

namespace sp {
    enum class GuiFont {
        Primary,
        Accent,
        Monospace,
    };

    struct GuiFontDef {
        GuiFont type;
        const char *name;
        float size;
    };

    class GuiContext {
    public:
        using Ref = std::shared_ptr<ecs::GuiRenderable>;

        GuiContext(const std::string &name);
        virtual ~GuiContext();
        void Attach(const Ref &component);
        void Detach(const Ref &component);
        void SetGuiContext();

        virtual void BeforeFrame();
        virtual void DefineWindows() = 0;

        const std::string &Name() const {
            return name;
        }

        static void PushFont(GuiFont fontType, float fontSize);

    protected:
        std::vector<Ref> components;

    private:
        ImGuiContext *imCtx = nullptr;
        std::string name;
    };

    std::shared_ptr<ecs::GuiRenderable> CreateGuiWindow(const std::string &name);

    std::span<GuiFontDef> GetGuiFontList();
} // namespace sp
