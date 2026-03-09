/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "ConsoleGui.hh"

#include "console/Console.hh"

#include <imgui.h>

namespace sp {
    static ImVec4 LogColours[] = {
        {1.0f, 0.6f, 0.4f, 1.0f},
        {1.0f, 0.9f, 0.4f, 1.0f},
        {0.8f, 0.8f, 0.8f, 1.0f},
        {0.5f, 0.5f, 0.6f, 1.0f},
    };

    ConsoleGui::ConsoleGui()
        : ecs::GuiDefinition("console",
              ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar) {}

    bool ConsoleGui::BeforeFrame(GenericCompositor &compositor, ecs::Entity ent) {
        return consoleOpen;
    }

    void ConsoleGui::DefineContents(ecs::Entity ent) {
        ZoneScoped;
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("ScrollingRegion",
            ImVec2(0, -footer_height_to_reserve),
            0,
            ImGuiWindowFlags_HorizontalScrollbar);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

        float frameHeight = ImGui::GetWindowHeight() - footer_height_to_reserve;
        float spacing = ImGui::GetTextLineHeightWithSpacing();
        float scrollY = ImGui::GetScrollY();
        GetConsoleManager().AccessLines([&](const std::vector<ConsoleLine> &lines) {
            size_t lineCount = lines.size();
            for (const ConsoleLine &line : lines) {
                lineCount += std::count(line.text.begin(), line.text.end(), '\n') - 1;
            }
            size_t minLine = (size_t)(std::max(scrollY - 1, 0.0f) / spacing);
            size_t maxLine = (size_t)((scrollY + spacing * 2 + frameHeight) / spacing);
            size_t startOffset = 0;
            float cursorOffset = minLine * spacing;
            for (size_t i = 0; i < minLine - startOffset && i < lines.size(); i++) {
                auto &line = lines[i];
                size_t delta = std::count(line.text.begin(), line.text.end(), '\n') - 1;
                if (delta > 0) {
                    startOffset += delta;
                    if (i >= minLine - startOffset) {
                        cursorOffset -= delta * spacing;
                    }
                }
            }
            float maxLineWidth = 0.0f;
            for (size_t i = minLine - startOffset; i <= maxLine && i < lines.size(); i++) {
                maxLineWidth = std::max(maxLineWidth, ImGui::CalcTextSize(lines[i].text.c_str()).x);
            }
            ImGui::BeginChild("ConsoleLinesArea", ImVec2(maxLineWidth, lineCount * spacing + 1));
            if (minLine > 0) ImGui::SetCursorPosY(cursorOffset);

            for (size_t i = minLine - startOffset; i <= maxLine && i < lines.size(); i++) {
                auto &line = lines[i];
                ImGui::PushStyleColor(ImGuiCol_Text, LogColours[(int)line.level]);
                ImGui::TextUnformatted(line.text.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::EndChild();
        });

        if (ImGui::GetScrollY() >= lastScrollMaxY - 0.001f) {
            ImGui::SetScrollHereY(1.0f);
        }

        lastScrollMaxY = ImGui::GetScrollMaxY();

        ImGui::PopStyleVar();
        ImGui::EndChild();

        bool reclaimInputFocus = ImGui::IsWindowAppearing();
        ImGuiInputTextFlags iflags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion |
                                     ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackAlways;

        if (ImGui::InputText("##CommandInput", inputBuf, sizeof(inputBuf), iflags, CommandEditStub, (void *)this)) {
            std::string line(inputBuf);
            if (!line.empty()) {
                auto &console = GetConsoleManager();
                console.AddHistory(line);
                console.QueueParseAndExecute(line);
                inputBuf[0] = '\0';
                completionMode = COMPLETION_NONE;
                completionPopupVisible = false;
            }
            reclaimInputFocus = true;
        }

        if (ImGui::IsItemEdited() && !skipEditCheck) {
            requestNewCompletions = true;
            completionPending = true;
            completionSelectedIndex = 0;
            completionSelectionChanged = true;
            completionPopupVisible = false;
            if (inputBuf[0] != '\0') {
                completionMode = COMPLETION_INPUT;
            } else {
                completionMode = COMPLETION_NONE;
            }
        } else {
            requestNewCompletions = false;
        }
        skipEditCheck = false;

        ImGui::SetItemDefaultFocus();
        if (reclaimInputFocus) ImGui::SetKeyboardFocusHere(-1);

        popupPos = ImGui::GetItemRectMin();
    }

    void ConsoleGui::PostDefine(ecs::Entity ent) {
        ZoneScoped;
        if (completionMode == COMPLETION_INPUT && completionPending) {
            std::string line(inputBuf);
            auto result = GetConsoleManager().AllCompletions(line, requestNewCompletions);
            completionPending = result.pending;
            completionEntries = result.values;
            completionPopupVisible = !completionEntries.empty();
        }

        if (completionPopupVisible) {
            ImGuiWindowFlags popupFlags = ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoTitleBar |
                                          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoSavedSettings;

            ImVec2 size = {400, 200};
            size.y = std::min(size.y, 12 + completionEntries.size() * ImGui::GetTextLineHeightWithSpacing());

            ImGui::SetNextWindowPos({popupPos.x, popupPos.y - size.y});
            ImGui::SetNextWindowSize(size);
            ImGui::Begin("completion_popup", nullptr, popupFlags);
            ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);

            for (int index = completionEntries.size() - 1; index >= 0; index--) {
                bool active = completionSelectedIndex == index;

                if (ImGui::Selectable(completionEntries[index].c_str(), active)) {
                    completionSelectedIndex = index;
                    syncInputFromCompletion = true;
                }
                if (active && completionSelectionChanged) {
                    ImGui::SetScrollHereY();
                    completionSelectionChanged = false;
                }
            }

            ImGui::PopItemFlag();
            ImGui::End();
            windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        } else {
            windowFlags &= ~ImGuiWindowFlags_NoBringToFrontOnFocus;
        }
    }

    int ConsoleGui::CommandEditStub(ImGuiInputTextCallbackData *data) {
        auto c = (ConsoleGui *)data->UserData;
        return c->CommandEditCallback(data);
    }

    void ConsoleGui::SetInput(ImGuiInputTextCallbackData *data, const char *str, bool skipEditCheck) {
        int newLength = snprintf(data->Buf, (size_t)data->BufSize, "%s", str);
        data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = newLength;
        data->BufDirty = true;
        this->skipEditCheck = skipEditCheck;
    }

    int ConsoleGui::CommandEditCallback(ImGuiInputTextCallbackData *data) {
        if ((data->EventFlag == ImGuiInputTextFlags_CallbackAlways && syncInputFromCompletion) ||
            data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
            if (completionSelectedIndex >= 0 && completionSelectedIndex < (int)completionEntries.size()) {
                std::string line = completionEntries[completionSelectedIndex];
                if (line[line.size() - 1] != ' ') line += " ";

                SetInput(data, line.c_str(), completionMode == COMPLETION_HISTORY);
                completionPopupVisible = false;
                completionSelectedIndex = -1;
            }
            syncInputFromCompletion = false;
        } else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            if (completionMode == COMPLETION_NONE) {
                completionEntries = GetConsoleManager().AllHistory(128);
                if (!completionEntries.empty()) {
                    completionMode = COMPLETION_HISTORY;
                    completionSelectedIndex = 0;
                    completionSelectionChanged = true;
                    completionPopupVisible = true;
                }
            } else if (data->EventKey == ImGuiKey_UpArrow) {
                if (completionSelectedIndex < (int)completionEntries.size() - 1) {
                    completionSelectedIndex++;
                    completionSelectionChanged = true;
                }
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (completionSelectedIndex > 0) {
                    completionSelectedIndex--;
                    completionSelectionChanged = true;
                } else if (completionMode == COMPLETION_HISTORY) {
                    SetInput(data, "", false);
                    completionMode = COMPLETION_NONE;
                    completionPopupVisible = false;
                }
            }

            if (completionMode == COMPLETION_HISTORY && completionSelectedIndex >= 0 &&
                completionSelectedIndex < (int)completionEntries.size())
                SetInput(data, completionEntries[completionSelectedIndex].c_str(), true);
        }
        return 0;
    }
} // namespace sp
