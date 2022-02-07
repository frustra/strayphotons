#include "console/Console.hh"

#include <imgui/imgui.h>

namespace sp {
    static ImVec4 LogColours[] = {
        {1.0f, 0.6f, 0.4f, 1.0f},
        {1.0f, 0.9f, 0.4f, 1.0f},
        {0.8f, 0.8f, 0.8f, 1.0f},
        {0.5f, 0.5f, 0.6f, 1.0f},
    };

    class ConsoleGui {
    public:
        void Add() {
            ImGuiIO &io = ImGui::GetIO();

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoTitleBar;

            if (completionPopupVisible) flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 400.0f));
            ImVec2 popupPos;

            ImGui::Begin("Console", nullptr, flags);
            {
                const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y +
                                                       ImGui::GetFrameHeightWithSpacing();
                ImGui::BeginChild("ScrollingRegion",
                    ImVec2(0, -footer_height_to_reserve),
                    false,
                    ImGuiWindowFlags_HorizontalScrollbar);

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

                for (auto &line : GetConsoleManager().Lines()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, LogColours[(int)line.level]);
                    ImGui::TextUnformatted(line.text.c_str());
                    ImGui::PopStyleColor();
                }

                if (ImGui::GetScrollY() >= lastScrollMaxY - 0.001f && io.MouseWheel == 0.0f) {
                    ImGui::SetScrollHereY();
                }

                lastScrollMaxY = ImGui::GetScrollMaxY();

                ImGui::PopStyleVar();
                ImGui::EndChild();

                ImGuiInputTextFlags iflags = ImGuiInputTextFlags_EnterReturnsTrue |
                                             ImGuiInputTextFlags_CallbackCompletion |
                                             ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackAlways;

                reclaimInputFocus |= ImGui::IsWindowAppearing();

                if (ImGui::InputText("##CommandInput",
                        inputBuf,
                        sizeof(inputBuf),
                        iflags,
                        CommandEditStub,
                        (void *)this)) {
                    string line(inputBuf);
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
                    string line(inputBuf);
                    if (!line.empty()) {
                        completionEntries = GetConsoleManager().AllCompletions(line);
                        completionMode = COMPLETION_INPUT;
                    } else {
                        completionEntries.clear();
                        completionMode = COMPLETION_NONE;
                    }
                    completionPopupVisible = !completionEntries.empty();
                    completionSelectedIndex = 0;
                    completionSelectionChanged = true;
                }
                skipEditCheck = false;

                ImGui::SetItemDefaultFocus();
                if (reclaimInputFocus) {
                    ImGui::SetKeyboardFocusHere(-1);
                    reclaimInputFocus = false;
                }

                popupPos = ImGui::GetItemRectMin();
            }
            ImGui::End();

            if (completionPopupVisible) {
                ImGuiWindowFlags popupFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_HorizontalScrollbar |
                                              ImGuiWindowFlags_NoSavedSettings;

                ImVec2 size = {400, 200};
                size.y = std::min(size.y, 12 + completionEntries.size() * ImGui::GetTextLineHeightWithSpacing());

                ImGui::SetNextWindowPos({popupPos.x, popupPos.y - size.y});
                ImGui::SetNextWindowSize(size);
                ImGui::Begin("completion_popup", nullptr, popupFlags);
                ImGui::PushAllowKeyboardFocus(false);

                if (completionSelectionChanged) reclaimInputFocus = true;

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

                ImGui::PopAllowKeyboardFocus();
                ImGui::End();
            }
        }

        static int CommandEditStub(ImGuiInputTextCallbackData *data) {
            auto c = (ConsoleGui *)data->UserData;
            return c->CommandEditCallback(data);
        }

        void SetInput(ImGuiInputTextCallbackData *data, const char *str, bool skipEditCheck) {
            int newLength = snprintf(data->Buf, (size_t)data->BufSize, "%s", str);
            data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = newLength;
            data->BufDirty = true;
            this->skipEditCheck = skipEditCheck;
        }

        int CommandEditCallback(ImGuiInputTextCallbackData *data) {
            if ((data->EventFlag == ImGuiInputTextFlags_CallbackAlways && syncInputFromCompletion) ||
                data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
                if (completionSelectedIndex >= 0 && completionSelectedIndex < (int)completionEntries.size()) {
                    string line = completionEntries[completionSelectedIndex];
                    if (line[line.size() - 1] != ' ') line += " ";

                    SetInput(data, line.c_str(), true);
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

    private:
        enum CompletionMode { COMPLETION_NONE, COMPLETION_INPUT, COMPLETION_HISTORY };

        float lastScrollMaxY = 0.0f;
        char inputBuf[1024];
        bool skipEditCheck = false, reclaimInputFocus = false;

        CompletionMode completionMode = COMPLETION_NONE;
        bool completionPopupVisible = false, completionSelectionChanged = false, syncInputFromCompletion = false;
        vector<string> completionEntries;
        int completionSelectedIndex = 0;
    };
} // namespace sp
