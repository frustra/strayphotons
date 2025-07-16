/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
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

    class InspectorGui final : public GuiRenderable {
    public:
        InspectorGui(const std::string &name);
        virtual ~InspectorGui() {}

        bool PreDefine() override;
        void DefineContents() override;
        void PostDefine() override;

    private:
        ecs::EventQueueRef events = ecs::EventQueue::New();
        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");
        bool stagingTabSelected = false;
        bool targetStagingEntity = false;
        ecs::EntityRef targetEntity;
        std::shared_ptr<Scene> targetScene;

        std::shared_ptr<EditorContext> context;
    };
} // namespace sp
