/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EventQueue.hh"
#include "ecs/components/GuiElement.hh"
#include "graphics/GenericCompositor.hh"

#include <memory>
#include <vector>

struct ImGuiContext;

namespace sp {
    struct GuiDrawData;
    class GenericCompositor;

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

    class GuiContext : public NonCopyable {
    public:
        GuiContext(const ecs::EntityRef &guiEntity);
        GuiContext(GuiContext &&other);
        virtual ~GuiContext();

        void ClearEntities();
        void AddEntity(ecs::Entity guiElementEntity,
            std::shared_ptr<ecs::GuiDefinition> definition,
            ecs::GuiLayoutAnchor anchor = ecs::GuiLayoutAnchor::Floating,
            glm::ivec2 preferredSize = {-100, -100});
        void Attach(std::shared_ptr<ecs::GuiDefinition> definition,
            ecs::GuiLayoutAnchor anchor = ecs::GuiLayoutAnchor::Floating,
            glm::ivec2 preferredSize = {-100, -100});
        void Detach(std::shared_ptr<ecs::GuiDefinition> definition);

        virtual bool SetGuiContext();
        virtual bool BeforeFrame(GenericCompositor &compositor);
        virtual void DefineWindows();
        virtual void GetDrawData(GuiDrawData &output) const;

        static void PushFont(GuiFont fontType, float fontSize);

    protected:
        ImGuiContext *imCtx = nullptr;

        ecs::EntityRef guiEntity;
        ecs::EventQueueRef events = ecs::EventQueue::New();

        struct GuiElementInfo {
            ecs::Entity ent;
            ecs::GuiLayoutAnchor anchor;
            glm::ivec2 preferredSize;
            std::shared_ptr<ecs::GuiDefinition> definition;
            bool enabled;
        };

        std::vector<GuiElementInfo> elements;

        struct PointingState {
            ecs::Entity sourceEntity;
            glm::vec2 mousePos;
            bool mouseDown = false;
        };

        vector<PointingState> pointingStack;
    };

    std::span<GuiFontDef> GetGuiFontList();
} // namespace sp
