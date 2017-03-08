#include "MenuGuiManager.hh"

#include "assets/AssetManager.hh"
#include "graphics/Texture.hh"
#include "graphics/GraphicsManager.hh"
#include "graphics/GraphicsContext.hh"
#include "game/InputManager.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"
#include "core/Console.hh"
#include "core/Game.hh"

#include <sstream>
#include <imgui/imgui.h>

namespace sp
{
	static CVar<bool> CVarMenuFocused("g.MenuFocused", false, "Focus input on menu");
	static CVar<int> CVarMenuDisplay("g.MenuDisplay", 0, "Display pause menu");

	void MenuGuiManager::BindInput(InputManager &input)
	{
		SetGuiContext();
		ImGuiIO &io = ImGui::GetIO();
		inputManager = &input;

		io.MousePos = ImVec2(200, 200);

		input.AddKeyInputCallback([&](int key, int state)
		{
			if (state == GLFW_PRESS)
				io.KeysDown[key] = true;
			if (state == GLFW_RELEASE)
				io.KeysDown[key] = false;

			io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
			io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
			io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
			io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];

			if (state == GLFW_PRESS && Focused() && !inputManager->FocusLocked(FocusLevel))
			{
				if (key == GLFW_KEY_ESCAPE && framesSinceOpened > 0)
				{
					if (selectedScreen == MenuScreen::Main && RenderMode() == MenuRenderMode::Pause)
					{
						CloseMenu();
					}

					selectedScreen = MenuScreen::Main;
				}

				if (key == GLFW_KEY_ENTER && selectedScreen == MenuScreen::Splash)
				{
					selectedScreen = MenuScreen::Main;
				}
			}
		});
	}

	void MenuGuiManager::BeforeFrame()
	{
		framesSinceOpened++;

		ImGuiIO &io = ImGui::GetIO();
		io.MouseDrawCursor = selectedScreen != MenuScreen::Splash && RenderMode() == MenuRenderMode::Gel;

		inputManager->LockFocus(Focused(), FocusLevel);

		if (Focused() && !inputManager->FocusLocked(FocusLevel))
		{
			auto &input = *inputManager;

			for (int i = 0; i < 3; i++)
			{
				io.MouseDown[i] = input.IsDown(MouseButtonToKey(i));
			}

			if (selectedScreen != MenuScreen::Splash)
			{
				io.MouseWheel = input.ScrollOffset().y;

				if (RenderMode() == MenuRenderMode::Gel)
				{
					auto cursorDiff = input.CursorDiff() * 2.0f;
					io.MousePos.x = std::max(std::min(io.MousePos.x + cursorDiff.x, io.DisplaySize.x), 0.0f);
					io.MousePos.y = std::max(std::min(io.MousePos.y + cursorDiff.y, io.DisplaySize.y), 0.0f);
				}
				else
				{
					auto guiCursorPos = input.ImmediateCursor();
					io.MousePos = ImVec2(guiCursorPos.x, guiCursorPos.y);
				}
			}
		}
	}

	static bool StringVectorGetter(void *data, int idx, const char **out_text)
	{
		auto vec = (vector<string> *) data;
		if (out_text)
		{
			*out_text = vec->at(idx).c_str();
		}
		return true;
	}

	static bool IsAspect(glm::ivec2 size, int w, int h)
	{
		return ((size.x * h) / w) == size.y;
	}

	static vector<string> MakeResolutionLabels(const vector<glm::ivec2> &modes)
	{
		vector<string> labels;
		for (size_t i = 0; i < modes.size(); i++)
		{
			auto m = modes[i];
			std::stringstream str;
			str << m.x << "x" << m.y;

			if (IsAspect(m, 16, 9)) str << " (16:9)";
			if (IsAspect(m, 16, 10)) str << " (16:10)";
			if (IsAspect(m, 4, 3)) str << " (4:3)";

			labels.push_back(str.str());
		}
		return labels;
	}

	void MenuGuiManager::DefineWindows()
	{
		ImGuiIO &io = ImGui::GetIO();

		//ImGui::ShowTestWindow();

		ImVec4 empty(0.0, 0.0, 0.0, 0.0);
		ImVec4 black(0.0, 0.0, 0.0, 1.0);
		ImVec4 white(1.0, 1.0, 1.0, 1.0);
		ImVec4 green(0.05, 1.0, 0.3, 1.0);

		ImGui::PushStyleColor(ImGuiCol_WindowBg, empty);
		ImGui::PushStyleColor(ImGuiCol_Button, empty);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, green);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, green);
		ImGui::PushStyleColor(ImGuiCol_Text, white);
		ImGui::PushStyleColor(ImGuiCol_TextButtonHover, black);
		ImGui::PushStyleColor(ImGuiCol_TextButtonActive, black);
		ImGui::PushStyleColor(ImGuiCol_CheckMark, green);
		ImGui::PushStyleColor(ImGuiCol_ComboBg, black);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0);
		ImGui::PushFont(io.Fonts->Fonts[2]);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_AlwaysAutoResize;

		static Texture logoTex = GAssets.LoadTexture("logos/sp-menu.png");
		static ImVec2 logoSize(logoTex.width * 0.5, logoTex.height * 0.5);

		if (selectedScreen == MenuScreen::Splash)
		{
			ImGui::SetNextWindowPosCenter(ImGuiSetCond_Always);
			ImGui::Begin("MenuSplash", nullptr, flags);
			ImGui::Text("Press Enter");
			ImGui::End();
		}
		else if (selectedScreen == MenuScreen::Main)
		{
			ImGui::SetNextWindowPosCenter(ImGuiSetCond_Always);
			ImGui::Begin("MenuMain", nullptr, flags);

			ImGui::Image((void *)(uintptr_t) logoTex.handle, logoSize);

			if (ImGui::Button(RenderMode() == MenuRenderMode::Pause ? "Resume" : "Start Game"))
			{
				CloseMenu();
			}

			if (ImGui::Button("Scene Select"))
			{
				selectedScreen = MenuScreen::SceneSelect;
			}

			if (ImGui::Button("Options"))
			{
				selectedScreen = MenuScreen::Options;
			}

			if (ImGui::Button("Quit"))
			{
				GConsoleManager.ParseAndExecute("exit");
			}

			ImGui::End();
		}
		else if (selectedScreen == MenuScreen::SceneSelect)
		{
			ImGui::SetNextWindowPosCenter(ImGuiSetCond_Always);
			ImGui::Begin("MenuSceneSelect", nullptr, flags);

			ImGui::Image((void *)(uintptr_t) logoTex.handle, logoSize);

			ImGui::Text("Scene Select");
			ImGui::Text(" ");

			ImGui::PushFont(io.Fonts->Fonts[3]);

			if (ImGui::Button("Test1"))
			{
				CloseMenu();
				GConsoleManager.ParseAndExecute("loadscene test1");
			}

			if (ImGui::Button("Level1"))
			{
				CloseMenu();
				GConsoleManager.ParseAndExecute("loadscene level1");
			}

			if (ImGui::Button("Sponza"))
			{
				CloseMenu();
				GConsoleManager.ParseAndExecute("loadscene sponza");
			}

			ImGui::PopFont();
			ImGui::Text(" ");

			if (ImGui::Button("Back"))
			{
				selectedScreen = MenuScreen::Main;
			}

			ImGui::End();
		}
		else if (selectedScreen == MenuScreen::Options)
		{
			ImGui::SetNextWindowPosCenter(ImGuiSetCond_Always);
			ImGui::Begin("MenuOptions", nullptr, flags);

			ImGui::Image((void *)(uintptr_t) logoTex.handle, logoSize);

			ImGui::Text("Options");
			ImGui::Text(" ");
			ImGui::Columns(2, "optcols", false);

			ImGui::PushFont(io.Fonts->Fonts[3]);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 15));

			ImGui::Text("Resolution");
			ImGui::Text("Full Screen");

			ImGui::PopStyleVar();
			ImGui::NextColumn();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10));

			{
				static auto modes = game->graphics.GetContext()->MonitorModes();
				static vector<string> resLabels = MakeResolutionLabels(modes);

				ImGui::PushItemWidth(250.0f);
				int resIndex = std::find(modes.begin(), modes.end(), CVarWindowSize.Get()) - modes.begin();
				ImGui::Combo("##respicker", &resIndex, StringVectorGetter, &resLabels, modes.size());
				ImGui::PopItemWidth();

				if (resIndex >= 0 && resIndex < (int) modes.size())
				{
					CVarWindowSize.Set(modes[resIndex]);
				}
			}

			{
				bool fullscreen = CVarWindowFullscreen.Get();
				ImGui::Checkbox("##fullscreencheck", &fullscreen);
				CVarWindowFullscreen.Set((int) fullscreen);
			}

			ImGui::PopStyleVar();
			ImGui::PopFont();
			ImGui::Columns(1);
			ImGui::Text(" ");

			if (ImGui::Button("Done"))
			{
				selectedScreen = MenuScreen::Main;
			}

			ImGui::End();
		}

		ImGui::PopFont();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(9);
	}

	bool MenuGuiManager::Focused()
	{
		return CVarMenuFocused.Get();
	}

	MenuRenderMode MenuGuiManager::RenderMode()
	{
		switch (CVarMenuDisplay.Get())
		{
			case 1:
				return MenuRenderMode::Pause;
			case 2:
				return MenuRenderMode::Gel;
		}
		return MenuRenderMode::None;
	}

	void MenuGuiManager::SetRenderMode(MenuRenderMode mode)
	{
		CVarMenuDisplay.Set((int) mode);
	}

	void MenuGuiManager::OpenPauseMenu()
	{
		if (RenderMode() == MenuRenderMode::None)
		{
			inputManager->EnableCursor();

			SetRenderMode(MenuRenderMode::Pause);
			selectedScreen = MenuScreen::Main;

			CVarMenuFocused.Set(true);
			inputManager->LockFocus(true, FocusLevel);
			framesSinceOpened = 0;
		}
	}

	void MenuGuiManager::CloseMenu()
	{
		if (!inputManager->FocusLocked(FocusLevel) && RenderMode() != MenuRenderMode::Gel)
		{
			inputManager->DisableCursor();
		}

		if (RenderMode() == MenuRenderMode::Pause)
		{
			SetRenderMode(MenuRenderMode::None);
			selectedScreen = MenuScreen::Main;
		}

		CVarMenuFocused.Set(false);
		inputManager->LockFocus(false, FocusLevel);
		framesSinceOpened = 0;
	}
}
