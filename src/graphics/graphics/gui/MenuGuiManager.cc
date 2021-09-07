#include "MenuGuiManager.hh"

#include "GraphicsManager.hh"
#include "assets/AssetManager.hh"
#include "core/CVar.hh"
#include "core/Console.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/core/GraphicsContext.hh"
#include "graphics/core/Texture.hh"
#include "input/core/BindingNames.hh"

#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <sstream>

namespace sp {
    static CVar<int> CVarMenuDisplay("g.MenuDisplay", 0, "Display pause menu");
    static CVar<bool> CVarMenuDebugCursor("g.MenuDebugCursor", false, "Force the cursor to be drawn in menus");
    static CVar<glm::vec2> CVarMenuCursorScaling("g.MenuCursorScaling",
                                                 glm::vec2(1.435f, 0.805f),
                                                 "Scaling factor for menu cursor position");

    MenuGuiManager::MenuGuiManager(GraphicsManager &graphics)
        : GuiManager("menu_gui", MenuOpen() ? ecs::FocusLayer::MENU : ecs::FocusLayer::GAME), graphics(graphics) {
        {
            auto lock =
                ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::EventInput, ecs::FocusLock>>();

            auto gui = guiEntity.Get(lock);
            Assert(gui.Has<ecs::EventInput>(lock), "Expected menu_gui to start with an EventInput");

            auto &eventInput = gui.Get<ecs::EventInput>(lock);
            if (MenuOpen()) {
                eventInput.Register(INPUT_EVENT_MENU_BACK);
                eventInput.Register(INPUT_EVENT_MENU_ENTER);
            } else {
                eventInput.Register(INPUT_EVENT_MENU_OPEN);
            }
        }
    }

    void MenuGuiManager::BeforeFrame() {
        GuiManager::BeforeFrame();

        ImGui::StyleColorsClassic();

        ImGuiIO &io = ImGui::GetIO();

        bool focusChanged = false;
        {
            auto lock = ecs::World.StartTransaction<
                ecs::Read<ecs::Name, ecs::SignalBindings, ecs::SignalOutput, ecs::FocusLayer, ecs::FocusLock>,
                ecs::Write<ecs::EventInput>>();

            auto gui = guiEntity.Get(lock);
            if (gui.Has<ecs::EventInput>(lock)) {
                ecs::Event event;
                while (ecs::EventInput::Poll(lock, gui, INPUT_EVENT_MENU_OPEN, event)) {
                    selectedScreen = MenuScreen::Main;
                    SetRenderMode(MenuRenderMode::Pause);
                }
                while (ecs::EventInput::Poll(lock, gui, INPUT_EVENT_MENU_BACK, event)) {
                    if (selectedScreen == MenuScreen::Main) {
                        if (RenderMode() == MenuRenderMode::Pause) SetRenderMode(MenuRenderMode::None);
                    } else {
                        selectedScreen = MenuScreen::Main;
                    }
                }
                while (ecs::EventInput::Poll(lock, gui, INPUT_EVENT_MENU_ENTER, event)) {
                    if (selectedScreen == MenuScreen::Splash) selectedScreen = MenuScreen::Main;
                }

                auto &prevInput = gui.GetPrevious<ecs::EventInput>(lock);
                if (MenuOpen()) {
                    if (!prevInput.IsRegistered(INPUT_EVENT_MENU_BACK)) {
                        gui.Get<ecs::EventInput>(lock).Register(INPUT_EVENT_MENU_BACK);
                    }
                    if (!prevInput.IsRegistered(INPUT_EVENT_MENU_ENTER)) {
                        gui.Get<ecs::EventInput>(lock).Register(INPUT_EVENT_MENU_ENTER);
                    }
                    if (prevInput.IsRegistered(INPUT_EVENT_MENU_OPEN)) {
                        gui.Get<ecs::EventInput>(lock).Unregister(INPUT_EVENT_MENU_OPEN);
                    }
                } else {
                    if (prevInput.IsRegistered(INPUT_EVENT_MENU_BACK)) {
                        gui.Get<ecs::EventInput>(lock).Unregister(INPUT_EVENT_MENU_BACK);
                    }
                    if (prevInput.IsRegistered(INPUT_EVENT_MENU_ENTER)) {
                        gui.Get<ecs::EventInput>(lock).Unregister(INPUT_EVENT_MENU_ENTER);
                    }
                    if (!prevInput.IsRegistered(INPUT_EVENT_MENU_OPEN)) {
                        gui.Get<ecs::EventInput>(lock).Register(INPUT_EVENT_MENU_OPEN);
                    }
                }
            }

            auto newFocusLayer = MenuOpen() ? ecs::FocusLayer::MENU : ecs::FocusLayer::GAME;
            focusChanged = focusLayer != newFocusLayer;
            focusLayer = newFocusLayer;

            if (MenuOpen() && RenderMode() == MenuRenderMode::Gel) {
                auto windowSize = CVarWindowSize.Get();
                auto cursorScaling = CVarMenuCursorScaling.Get();
                io.MousePos.x = io.MousePos.x / (float)windowSize.x * io.DisplaySize.x;
                io.MousePos.y = io.MousePos.y / (float)windowSize.y * io.DisplaySize.y;
                io.MousePos.x = io.MousePos.x * cursorScaling.x - io.DisplaySize.x * (cursorScaling.x - 1.0f) * 0.5f;
                io.MousePos.y = io.MousePos.y * cursorScaling.y - io.DisplaySize.y * (cursorScaling.y - 1.0f) * 0.5f;
                io.MousePos.x = std::max(std::min(io.MousePos.x, io.DisplaySize.x), 0.0f);
                io.MousePos.y = std::max(std::min(io.MousePos.y, io.DisplaySize.y), 0.0f);
            }
        }

        io.MouseDrawCursor = selectedScreen != MenuScreen::Splash && RenderMode() == MenuRenderMode::Gel;
        io.MouseDrawCursor = io.MouseDrawCursor || CVarMenuDebugCursor.Get();

        if (focusChanged) {
            auto lock =
                ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::FocusLayer, ecs::FocusLock>>();

            auto gui = guiEntity.Get(lock);
            if (gui.Has<ecs::FocusLayer>(lock)) gui.Set<ecs::FocusLayer>(lock, focusLayer);
            auto &focusLock = lock.Get<ecs::FocusLock>();
            if (MenuOpen()) {
                focusLock.AcquireFocus(ecs::FocusLayer::MENU);
            } else {
                focusLock.ReleaseFocus(ecs::FocusLayer::MENU);
            }
        }
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

        static shared_ptr<GpuTexture> logoTex = graphics.GetContext()->LoadTexture(
            GAssets.LoadImage("logos/sp-menu.png"));
        static ImVec2 logoSize(logoTex->GetWidth() * 0.5f, logoTex->GetHeight() * 0.5f);

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

            ImGui::Image((void *)logoTex->GetHandle(), logoSize);

            if (ImGui::Button(RenderMode() == MenuRenderMode::Pause ? "Resume" : "Start Game")) {
                if (RenderMode() == MenuRenderMode::Gel) {
                    GetConsoleManager().QueueParseAndExecute("p.PausePlayerPhysics 0");
                }
                SetRenderMode(MenuRenderMode::None);
            }

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

            ImGui::Image((void *)logoTex->GetHandle(), logoSize);

            ImGui::Text("Scene Select");
            ImGui::Text(" ");

            ImGui::PushFont(io.Fonts->Fonts[3]);

#define LEVEL_BUTTON(name, file)                                                                                       \
    if (ImGui::Button(name)) {                                                                                         \
        SetRenderMode(MenuRenderMode::None);                                                                           \
        selectedScreen = MenuScreen::Main;                                                                             \
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

            ImGui::Image((void *)logoTex->GetHandle(), logoSize);

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
                    modes = graphics.GetContext()->MonitorModes();
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
                        size = graphics.GetContext()->CurrentMode();
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

            static shared_ptr<GpuTexture> frLogoTex = graphics.GetContext()->LoadTexture(
                GAssets.LoadImage("logos/credits-frustra.png"));
            static ImVec2 frLogoSize(frLogoTex->GetWidth() * 0.5f, frLogoTex->GetHeight() * 0.5f);

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
            ImGui::Image((void *)frLogoTex->GetHandle(), frLogoSize);
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

    MenuRenderMode MenuGuiManager::RenderMode() const {
        switch (CVarMenuDisplay.Get()) {
        case 1:
            return MenuRenderMode::Pause;
        case 2:
            return MenuRenderMode::Gel;
        }
        return MenuRenderMode::None;
    }

    bool MenuGuiManager::MenuOpen() const {
        switch (RenderMode()) {
        case MenuRenderMode::Pause:
        case MenuRenderMode::Gel:
            return true;
        default:
            return false;
        }
    }

    void MenuGuiManager::SetRenderMode(MenuRenderMode mode) {
        CVarMenuDisplay.Set((int)mode);
    }
} // namespace sp
