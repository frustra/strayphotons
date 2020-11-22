#include "core/Console.hh"

#include <imgui/imgui.h>

namespace sp {
    static ImVec4 LogColours[] = {
        {1.0f, 0.6f, 0.4f, 1.0f},
        {0.8f, 0.8f, 0.8f, 1.0f},
        {0.5f, 0.5f, 0.6f, 1.0f},
    };

    class ConsoleGui {
    public:
        void Add() {
            ImGuiIO &io = ImGui::GetIO();

            ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar;

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 400.0f));

            ImGui::Begin("Console", nullptr, flags);
            {
                const float footer_height_to_reserve =
                    ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
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

                if (ImGui::GetScrollY() >= lastScrollMaxY - 0.001f && io.MouseWheel == 0.0f) { ImGui::SetScrollHere(); }

                lastScrollMaxY = ImGui::GetScrollMaxY();

                ImGui::PopStyleVar();
                ImGui::EndChild();

                ImGuiInputTextFlags iflags = ImGuiInputTextFlags_EnterReturnsTrue |
                                             ImGuiInputTextFlags_CallbackCompletion |
                                             ImGuiInputTextFlags_CallbackHistory;

                bool reclaim_focus = ImGui::IsWindowAppearing();
                if (ImGui::InputText(
                        "##CommandInput", inputBuf, sizeof(inputBuf), iflags, CommandEditStub, (void *)this)) {
                    string line(inputBuf);
                    auto &console = GetConsoleManager();
                    console.AddHistory(line);
                    console.QueueParseAndExecute(line);
                    inputBuf[0] = '\0';
                    historyOffset = 0;
                    reclaim_focus = true;
                }

                ImGui::SetItemDefaultFocus();
                if (reclaim_focus) ImGui::SetKeyboardFocusHere(-1);
            }
            ImGui::End();
        }

        static int CommandEditStub(ImGuiTextEditCallbackData *data) {
            auto c = (ConsoleGui *)data->UserData;
            return c->CommandEditCallback(data);
        }

        int CommandEditCallback(ImGuiTextEditCallbackData *data) {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
                string line(data->Buf);
                line = GetConsoleManager().AutoComplete(line);

                int newLength = snprintf(data->Buf, (size_t)data->BufSize, "%s", line.c_str());
                data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = newLength;
                data->BufDirty = true;
                historyOffset = 0;
            } else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                int pos = historyOffset;

                if (data->EventKey == ImGuiKey_UpArrow) pos++;
                if (data->EventKey == ImGuiKey_DownArrow) pos--;

                if (pos != historyOffset) {
                    if (pos > 0) {
                        if (historyOffset == 0) {
                            memcpy(newInputBuf, inputBuf, sizeof(inputBuf));
                            newInputCursorOffset = data->CursorPos;
                        }

                        auto line = GetConsoleManager().GetHistory(pos);
                        if (!line.empty()) {
                            int newLength = snprintf(data->Buf, (size_t)data->BufSize, "%s", line.c_str());
                            data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = newLength;
                            data->BufDirty = true;
                            historyOffset = pos;
                        }
                    } else if (pos == 0) {
                        int newLength = snprintf(data->Buf, (size_t)data->BufSize, "%s", newInputBuf);
                        data->BufTextLen = newLength;
                        data->CursorPos = data->SelectionStart = data->SelectionEnd = newInputCursorOffset;
                        data->BufDirty = true;
                        historyOffset = pos;
                    }
                }
            }
            return 0;
        }

    private:
        float lastScrollMaxY = 0.0f;
        char inputBuf[1024], newInputBuf[1024];
        int historyOffset = 0;
        int newInputCursorOffset = 0;
    };
} // namespace sp
