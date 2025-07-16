/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/EntityRef.hh"
#include "ecs/EventQueue.hh"
#include "graphics/gui/GuiContext.hh"

#include <memory>
#include <string>

namespace sp {
    struct EditorContext;
    class Scene;

    class EntityPickerGui final : public GuiRenderable {
    public:
        EntityPickerGui(const std::string &name);
        virtual ~EntityPickerGui() {}

        bool PreDefine() override;
        void DefineContents() override;
        void PostDefine() override;

    private:
        ecs::EventQueueRef events = ecs::EventQueue::New();
        ecs::EntityRef pickerEntity = ecs::Name("editor", "entity_picker");
        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");
        ecs::EntityRef targetEntity;
        std::shared_ptr<Scene> targetScene;

        std::shared_ptr<EditorContext> context;
    };
} // namespace sp
