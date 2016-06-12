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
			}
			ImGui::End();

			ImGui::PopStyleVar();
		}

	private:
		float lastScrollMaxY = 0.0f;
	};
}