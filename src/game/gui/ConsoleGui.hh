#include "core/Console.hh"

namespace sp
{
	static ImVec4 LogColours[] =
	{
		{1.0f, 0.6f, 0.4f, 1.0f},
		{0.8f, 0.8f, 0.8f, 1.0f},
		{0.5f, 0.5f, 0.6f, 1.0f},
	};

	class ConsoleGui
	{
	public:
		void Add()
		{
			ImGuiIO &io = ImGui::GetIO();

			ImGuiWindowFlags flags =
				ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoTitleBar;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::SetNextWindowPos(ImVec2(0, 0));

			ImGui::Begin("Console", nullptr, ImVec2(io.DisplaySize.x, 400.0f), 0.7f, flags);
			{
				ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);

				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

				for (auto &line : GConsoleManager.Lines())
				{
					ImGui::PushStyleColor(ImGuiCol_Text, LogColours[(int)line.level]);
					ImGui::TextUnformatted(line.text.c_str());
					ImGui::PopStyleColor();
				}

				if (ImGui::GetScrollY() >= lastScrollMaxY - 0.1f)
				{
					ImGui::SetScrollHere();
				}

				lastScrollMaxY = ImGui::GetScrollMaxY();

				ImGui::PopStyleVar();
				ImGui::EndChild();

				ImGuiInputTextFlags iflags =
					ImGuiInputTextFlags_EnterReturnsTrue |
					ImGuiInputTextFlags_CallbackCompletion |
					ImGuiInputTextFlags_CallbackHistory;

				if (ImGui::InputText("##CommandInput", inputBuf, sizeof(inputBuf), iflags, CommandEditStub, (void *) this))
				{
					string line(inputBuf);
					GConsoleManager.ParseAndExecute(line);
					inputBuf[0] = '\0';
					historyOffset = 0;
				}

				if (ImGui::IsItemHovered() || (ImGui::IsRootWindowOrAnyChildFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
					ImGui::SetKeyboardFocusHere(-1);
			}
			ImGui::End();

			ImGui::PopStyleVar();
		}

		static int CommandEditStub(ImGuiTextEditCallbackData *data)
		{
			auto c = (ConsoleGui *) data->UserData;
			return c->CommandEditCallback(data);
		}

		int CommandEditCallback(ImGuiTextEditCallbackData *data)
		{
			if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
			{
				string line(data->Buf);
				line = GConsoleManager.AutoComplete(line);

				int newLength = snprintf(data->Buf, (size_t)data->BufSize, "%s", line.c_str());
				data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = newLength;
				data->BufDirty = true;
				historyOffset = 0;
			}
			else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
			{
				int pos = historyOffset;

				if (data->EventKey == ImGuiKey_UpArrow) pos++;
				if (data->EventKey == ImGuiKey_DownArrow) pos--;

				if (pos != historyOffset)
				{
					if (pos > 0)
					{
						if (historyOffset == 0)
						{
							memcpy(newInputBuf, inputBuf, sizeof(inputBuf));
							newInputCursorOffset = data->CursorPos;
						}

						auto hist = GConsoleManager.History();
						if (pos <= hist.size())
						{
							auto &line = hist[hist.size() - pos];
							int newLength = snprintf(data->Buf, (size_t)data->BufSize, "%s", line.c_str());
							data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = newLength;
							data->BufDirty = true;
							historyOffset = pos;
						}
					}
					else if (pos == 0)
					{
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
}