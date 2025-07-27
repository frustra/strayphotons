/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "console/Console.hh"
#include "ecs/components/Gui.hh"

#include <imgui/imgui.h>

namespace sp {
    static ImVec4 LogColours[] = {
        {1.0f, 0.6f, 0.4f, 1.0f},
        {1.0f, 0.9f, 0.4f, 1.0f},
        {0.8f, 0.8f, 0.8f, 1.0f},
        {0.5f, 0.5f, 0.6f, 1.0f},
    };

    class ConsoleGui final : public ecs::GuiRenderable {
    public:
        ConsoleGui();

        bool PreDefine(ecs::Entity ent) override;
        void DefineContents(ecs::Entity ent) override;
        void PostDefine(ecs::Entity ent) override;

        static int CommandEditStub(ImGuiInputTextCallbackData *data);

        void SetInput(ImGuiInputTextCallbackData *data, const char *str, bool skipEditCheck);

        int CommandEditCallback(ImGuiInputTextCallbackData *data);

        bool consoleOpen = false;

    private:
        enum CompletionMode { COMPLETION_NONE, COMPLETION_INPUT, COMPLETION_HISTORY };

        float lastScrollMaxY = 0.0f;
        char inputBuf[1024] = {};
        bool skipEditCheck = false;

        ImVec2 popupPos;
        CompletionMode completionMode = COMPLETION_NONE;
        bool completionPopupVisible = false, completionSelectionChanged = false, syncInputFromCompletion = false,
             completionPending = false, requestNewCompletions = false;
        vector<string> completionEntries;
        int completionSelectedIndex = 0;
    };
} // namespace sp
