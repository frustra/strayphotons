/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "game/SceneRef.hh"

namespace sp {
    struct EditorContext {
        struct TreeNode {
            bool hasParent = false;
            std::vector<ecs::Name> children;
        };

        // Persistent context
        std::string entitySearch, sceneEntry, signalSearch;
        std::map<ecs::Name, TreeNode> entityTree;
        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");
        const ecs::ComponentBase *selectedComponent = nullptr;
        SceneRef scene;
        ecs::SignalRef selectedSignal;
        ecs::Entity target;
        std::string followFocus;
        int followFocusPos;

        // Temporary context
        const ecs::Lock<ecs::ReadAll> *lock = nullptr;
        std::string fieldName, fieldId;
        int signalNameCursorPos;

        void RefreshEntityTree();
        bool ShowEntityTree(ecs::EntityRef &selected, ecs::Name = ecs::Name());
        bool ShowAllEntities(ecs::EntityRef &selected,
            std::string listLabel,
            float listWidth = -FLT_MIN,
            float listHeight = -FLT_MIN);
        void AddLiveSignalControls(const ecs::Lock<ecs::ReadAll> &lock, const ecs::EntityRef &targetEntity);
        void ShowEntityControls(const ecs::Lock<ecs::ReadAll> &lock, const ecs::EntityRef &targetEntity);
        void ShowSceneControls(const ecs::Lock<ecs::ReadAll> &lock);
        void ShowSignalControls(const ecs::Lock<ecs::ReadAll> &lock);

        template<typename T>
        bool AddImGuiElement(const std::string &name, T &value);
        template<typename T>
        void AddFieldControls(const ecs::StructField &field, const ecs::ComponentBase &comp, const void *component);
    };

    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    void CopyToStaging(Tecs::Lock<ECSType<AllComponentTypes...>, ecs::AddRemove> staging,
        ecs::Lock<ecs::ReadAll> live,
        ecs::Entity target);
} // namespace sp
