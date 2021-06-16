#include "MenuGuiManager.hh"

#include "assets/AssetManager.hh"
#include "core/CVar.hh"
#include "core/Console.hh"
#include "core/Logging.hh"
#include "graphics/GraphicsManager.hh"
#include "graphics/opengl/GLTexture.hh"

#include <core/Game.hh>
#include <game/input/InputManager.hh>
#include <graphics/glfw_graphics_context/GlfwGraphicsContext.hh>
#include <imgui/imgui.h>
#include <sstream>

namespace sp {
    static CVar<bool> CVarMenuFocused("g.MenuFocused", false, "Focus input on menu");
    static CVar<int> CVarMenuDisplay("g.MenuDisplay", 0, "Display pause menu");
    static CVar<bool> CVarMenuDebugCursor("g.MenuDebugCursor", false, "Force the cursor to be drawn in menus");

    MenuGuiManager::MenuGuiManager(Game *game) : GuiManager(game, FOCUS_MENU) {
        SetGuiContext();
        ImGuiIO &io = ImGui::GetIO();
        io.MousePos = ImVec2(200, 200);
    }

    void MenuGuiManager::BeforeFrame() {
        GuiManager::BeforeFrame();

        ImGui::StyleColorsClassic();
        framesSinceOpened++;

        ImGuiIO &io = ImGui::GetIO();

        if (input != nullptr) {
            input->LockFocus(Focused(), focusPriority);

            if (Focused() && !input->FocusLocked(focusPriority)) {
                if (framesSinceOpened > 1 && input->IsPressed(INPUT_ACTION_MENU_BACK)) {
                    if (selectedScreen == MenuScreen::Main && RenderMode() == MenuRenderMode::Pause) { CloseMenu(); }

                    selectedScreen = MenuScreen::Main;
                }
                if (selectedScreen == MenuScreen::Splash) {
                    if (input->IsPressed(INPUT_ACTION_MENU_ENTER)) { selectedScreen = MenuScreen::Main; }
                }

                io.MouseDown[0] = input->IsDown(INPUT_ACTION_MOUSE_BASE + "/button_left");
                io.MouseDown[1] = input->IsDown(INPUT_ACTION_MOUSE_BASE + "/button_right");
                io.MouseDown[2] = input->IsDown(INPUT_ACTION_MOUSE_BASE + "/button_middle");

                const glm::vec2 *scrollOffset, *scrollOffsetPrev;
                if (input->GetActionDelta(INPUT_ACTION_MOUSE_SCROLL, &scrollOffset, &scrollOffsetPrev)) {
                    if (scrollOffsetPrev != nullptr) {
                        io.MouseWheel = scrollOffset->y - scrollOffsetPrev->y;
                    } else {
                        io.MouseWheel = scrollOffset->y;
                    }
                }

                if (selectedScreen != MenuScreen::Splash) {
                    if (RenderMode() == MenuRenderMode::Gel) {
                        const glm::vec2 *cursorPos, *cursorPosPrev;
                        if (input->GetActionDelta(sp::INPUT_ACTION_MOUSE_CURSOR, &cursorPos, &cursorPosPrev)) {
                            glm::vec2 cursorDiff = *cursorPos;
                            if (cursorPosPrev != nullptr) { cursorDiff -= *cursorPosPrev; }

                            // TODO: Configure this scale
                            cursorDiff *= 2.0f;
                            io.MousePos.x = std::max(std::min(io.MousePos.x + cursorDiff.x, io.DisplaySize.x), 0.0f);
                            io.MousePos.y = std::max(std::min(io.MousePos.y + cursorDiff.y, io.DisplaySize.y), 0.0f);
                        }
                    } else {
                        // auto guiCursorPos = input->ImmediateCursor();
                        const glm::vec2 *mousePos;
                        if (input->GetActionValue(INPUT_ACTION_MOUSE_CURSOR, &mousePos)) {
                            io.MousePos = ImVec2(mousePos->x, mousePos->y);
                        }
                    }
                }
            }
        }

        io.MouseDrawCursor = selectedScreen != MenuScreen::Splash && RenderMode() == MenuRenderMode::Gel;
        io.MouseDrawCursor = io.MouseDrawCursor || CVarMenuDebugCursor.Get();
    }

    static bool StringVectorGetter(void *data, int idx, const char **out_text) {
        auto vec = (vector<string> *)data;
        if (out_text) { *out_text = vec->at(idx).c_str(); }
        return true;
    }

    static bool IsAspect(glm::ivec2 size, int w, int h) {
        return ((size.x * h) / w) == size.y;
    }

    static vector<string> MakeResolutionLabels(const vector<glm::ivec2> &modes) {
        vector<string> labels;
        for (size_t i = 0; i < modes.size(); i++) {
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

    void MenuGuiManager::DefineWindows() {
        SetGuiContext();
        ImGuiIO &io = ImGui::GetIO();

        // ImGui::ShowTestWindow();

        ImVec4 empty(0.0, 0.0, 0.0, 0.0);
        ImVec4 black(0.0, 0.0, 0.0, 1.0);
        ImVec4 white(1.0, 1.0, 1.0, 1.0);
        ImVec4 green(0.05, 1.0, 0.3, 1.0);

        ImGui::PushStyleColor(ImGuiCol_Button, empty);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, green);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, green);
        ImGui::PushStyleColor(ImGuiCol_Text, white);
        ImGui::PushStyleColor(ImGuiCol_TextButtonHover, black);
        ImGui::PushStyleColor(ImGuiCol_TextButtonActive, black);
        ImGui::PushStyleColor(ImGuiCol_CheckMark, green);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, black);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0);
        ImGui::PushFont(io.Fonts->Fonts[2]);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_AlwaysAutoResize;

        static GLTexture logoTex = GLTexture().Create().LoadFromTexture(GAssets.LoadTexture("logos/sp-menu.png"));
        static ImVec2 logoSize(logoTex.width * 0.5, logoTex.height * 0.5);

        if (selectedScreen == MenuScreen::Splash) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                    ImGuiCond_Always,
                                    ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuSplash", nullptr, flags);
            ImGui::Text("Press Enter");
            ImGui::End();
        } else if (selectedScreen == MenuScreen::Main) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                    ImGuiCond_Always,
                                    ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuMain", nullptr, flags);

            ImGui::Image((void *)(uintptr_t)logoTex.handle, logoSize);

            if (ImGui::Button(RenderMode() == MenuRenderMode::Pause ? "Resume" : "Start Game")) { CloseMenu(); }

            if (ImGui::Button("Scene Select")) { selectedScreen = MenuScreen::SceneSelect; }

            if (ImGui::Button("Options")) { selectedScreen = MenuScreen::Options; }

            if (RenderMode() != MenuRenderMode::Pause && ImGui::Button("Credits")) {
                selectedScreen = MenuScreen::Credits;
                creditsScroll = 0.0f;
            }

            if (ImGui::Button("Quit")) { GetConsoleManager().QueueParseAndExecute("exit"); }

            ImGui::End();
        } else if (selectedScreen == MenuScreen::SceneSelect) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                    ImGuiCond_Always,
                                    ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuSceneSelect", nullptr, flags);

            ImGui::Image((void *)(uintptr_t)logoTex.handle, logoSize);

            ImGui::Text("Scene Select");
            ImGui::Text(" ");

            ImGui::PushFont(io.Fonts->Fonts[3]);

#define LEVEL_BUTTON(name, file)                                                                                       \
    if (ImGui::Button(name)) {                                                                                         \
        CloseMenu();                                                                                                   \
        GetConsoleManager().QueueParseAndExecute("loadscene " file);                                                   \
    }

            LEVEL_BUTTON("01 - Outside", "01-outside")
            LEVEL_BUTTON("02 - Mirrors", "02-mirrors")
            LEVEL_BUTTON("03 - Dark", "03-dark")
            LEVEL_BUTTON("04 - Symmetry", "04-symmetry")
            LEVEL_BUTTON("Sponza", "sponza")
            LEVEL_BUTTON("Cornell Box", "cornell-box-1")
            LEVEL_BUTTON("Cornell Box Mirror", "cornell-box-3")
            LEVEL_BUTTON("Test 1", "test1")

#undef LEVEL_BUTTON

            ImGui::PopFont();
            ImGui::Text(" ");

            if (ImGui::Button("Back")) { selectedScreen = MenuScreen::Main; }

            ImGui::End();
        } else if (selectedScreen == MenuScreen::Options) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                    ImGuiCond_Always,
                                    ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuOptions", nullptr, flags);

            ImGui::Image((void *)(uintptr_t)logoTex.handle, logoSize);

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
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0);

            {
                static std::vector<glm::ivec2> modes;
                auto size = CVarWindowSize.Get();
                static auto prevSize = size;
                int resIndex = std::find(modes.begin(), modes.end(), size) - modes.begin();
                // Check for valid resIndex.  Note: An invalid index may occur if the default window size is not a
                // supported graphics mode for the monitor.
                if (resIndex < 0 || resIndex >= (int)modes.size()) {
                    // If the mode isn't in the list, refresh it, and then add the current resolution to the bottom if
                    // not found.
                    modes = game->graphics.GetContext()->MonitorModes();
                    resIndex = std::find(modes.begin(), modes.end(), size) - modes.begin();
                    if (resIndex < 0 || resIndex >= (int)modes.size()) {
                        resIndex = modes.size();
                        modes.push_back(size);
                    }
                }

                {
                    static vector<string> resLabels = MakeResolutionLabels(modes);

                    ImGui::PushItemWidth(250.0f);
                    ImGui::Combo("##respicker", &resIndex, StringVectorGetter, &resLabels, modes.size());
                    ImGui::PopItemWidth();
                }

                bool fullscreen = CVarWindowFullscreen.Get();
                static bool prevFullscreen = fullscreen;
                ImGui::Checkbox("##fullscreencheck", &fullscreen);

                // If fullscreen is toggeled from the Menu GUI.
                if (prevFullscreen != fullscreen) {
                    if (fullscreen) {
                        // Store windowed mode resolution for returning from fullscreen.
                        prevSize = size;
                        // Resize the window to the current screen resolution.
                        size = game->graphics.GetContext()->CurrentMode();
                        if (size != glm::ivec2(0)) { CVarWindowSize.Set(size); }
                        CVarWindowFullscreen.Set(1);
                    } else {
                        CVarWindowFullscreen.Set(0);
                        CVarWindowSize.Set(prevSize);
                    }

                    prevFullscreen = fullscreen;
                } else if (size != modes[resIndex]) {
                    size = modes[resIndex];
                    CVarWindowSize.Set(size);
                }
            }

            ImGui::PopStyleVar(2);
            ImGui::PopFont();
            ImGui::Columns(1);
            ImGui::Text(" ");

            if (ImGui::Button("Done")) { selectedScreen = MenuScreen::Main; }

            ImGui::End();
        } else if (selectedScreen == MenuScreen::Credits) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                    ImGuiCond_Always,
                                    ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuCredits", nullptr, flags);

            static GLTexture frLogoTex = GLTexture().Create().LoadFromTexture(
                GAssets.LoadTexture("logos/credits-frustra.png"));
            static ImVec2 frLogoSize(frLogoTex.width * 0.5, frLogoTex.height * 0.5);

            ImGui::BeginChild("CreditScroller", ImVec2(600, 600), false, ImGuiWindowFlags_NoScrollbar);
            ImGui::SetScrollY(creditsScroll);

#define CenteredText(str)                                                                                              \
    {                                                                                                                  \
        auto size = ImGui::CalcTextSize((str));                                                                        \
        ImGui::Indent(300.0f - size.x / 2.0f);                                                                         \
        ImGui::Text((str));                                                                                            \
        ImGui::Unindent(300.0f - size.x / 2.0f);                                                                       \
    }

            ImGui::Dummy({1, 500});
            CenteredText("STRAY PHOTONS");
            CenteredText(" ");
            CenteredText("Copyright © 2017 Frustra Software");
            CenteredText(" ");

            ImGui::Indent(300.0f - frLogoSize.x / 2.0f);
            ImGui::Image((void *)(uintptr_t)frLogoTex.handle, frLogoSize);
            ImGui::Unindent(300.0f - frLogoSize.x / 2.0f);

            CenteredText(" ");
            CenteredText(" ");

            CenteredText("Development Team");
            CenteredText(" ");
            CenteredText("Jacob Wirth");
            CenteredText("Justin Li");
            CenteredText("Cory Stegelmeier");
            CenteredText("Kevin Jeong");
            CenteredText("Michael Noukhovitch");

            ImGui::Dummy({1, 100});

            ImGui::PushFont(io.Fonts->Fonts[3]);
            CenteredText("NVIDIA GameWorks™ Technology provided under");
            CenteredText("license from NVIDIA Corporation.");
            CenteredText("Copyright © 2002-2015 NVIDIA Corporation.");
            CenteredText("All rights reserved.");
            ImGui::PopFont();

            ImGui::Dummy({1, 600});

            creditsScroll += io.DeltaTime * 20.0f;
            if (creditsScroll >= ImGui::GetScrollMaxY() && creditsScroll > 100) { selectedScreen = MenuScreen::Main; }

#undef CenteredText

            ImGui::EndChild();
            ImGui::End();
        }

        ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(8);
    }

    bool MenuGuiManager::Focused() {
        return CVarMenuFocused.Get();
    }

    MenuRenderMode MenuGuiManager::RenderMode() {
        switch (CVarMenuDisplay.Get()) {
        case 1:
            return MenuRenderMode::Pause;
        case 2:
            return MenuRenderMode::Gel;
        }
        return MenuRenderMode::None;
    }

    void MenuGuiManager::SetRenderMode(MenuRenderMode mode) {
        CVarMenuDisplay.Set((int)mode);
    }

    void MenuGuiManager::OpenPauseMenu() {
        if (RenderMode() == MenuRenderMode::None) {
            auto gfxContext = game->graphics.GetContext();
            if (gfxContext) { gfxContext->EnableCursor(); }

            SetRenderMode(MenuRenderMode::Pause);
            selectedScreen = MenuScreen::Main;

            CVarMenuFocused.Set(true);
            if (input != nullptr) { input->LockFocus(true, focusPriority); }
            framesSinceOpened = 0;
        }
    }

    void MenuGuiManager::CloseMenu() {
        if (input != nullptr && !input->FocusLocked(focusPriority) && RenderMode() != MenuRenderMode::Gel) {
            auto gfxContext = game->graphics.GetContext();
            if (gfxContext) { gfxContext->DisableCursor(); }
        }

        if (RenderMode() == MenuRenderMode::Pause) {
            SetRenderMode(MenuRenderMode::None);
            selectedScreen = MenuScreen::Main;
        }

        CVarMenuFocused.Set(false);
        if (input != nullptr) { input->LockFocus(false, focusPriority); }
        framesSinceOpened = 0;
    }
} // namespace sp
