/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "MenuGuiManager.hh"

#include "assets/AssetManager.hh"
#include "common/Logging.hh"
#include "console/CVar.hh"
#include "console/Console.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/core/GraphicsContext.hh"
#include "graphics/core/GraphicsManager.hh"
#include "graphics/core/Texture.hh"
#include "input/BindingNames.hh"

#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <sstream>

namespace sp {
    static CVar<bool> CVarMenuOpen("g.MenuOpen", 0, "Display pause menu");
    static CVar<bool> CVarMenuDebugCursor("g.MenuDebugCursor", false, "Force the cursor to be drawn in menus");

    MenuGuiManager::MenuGuiManager(GraphicsManager &graphics) : SystemGuiManager("menu"), graphics(graphics) {
        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::EventInput, ecs::FocusLock>>();

            auto gui = guiEntity.Get(lock);
            Assertf(gui.Has<ecs::EventInput>(lock),
                "Expected menu gui to start with an EventInput: %s",
                guiEntity.Name().String());

            auto &eventInput = gui.Get<ecs::EventInput>(lock);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_OPEN);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_BACK);
        }
    }

    void MenuGuiManager::BeforeFrame() {
        SystemGuiManager::BeforeFrame();

        ImGui::StyleColorsClassic();

        ImGuiIO &io = ImGui::GetIO();

        bool focusChanged = false;
        {
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock, ecs::Read<ecs::EventInput>>();

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, events, event)) {
                if (event.name == INPUT_EVENT_MENU_OPEN) {
                    selectedScreen = MenuScreen::Main;
                    CVarMenuOpen.Set(true);
                } else if (event.name == INPUT_EVENT_MENU_BACK) {
                    if (selectedScreen == MenuScreen::Main) {
                        CVarMenuOpen.Set(false);
                    } else {
                        selectedScreen = MenuScreen::Main;
                    }
                }
            }

            auto &focusLock = lock.Get<ecs::FocusLock>();
            focusChanged = MenuOpen() != focusLock.HasFocus(ecs::FocusLayer::Menu);
        }

        io.MouseDrawCursor = CVarMenuDebugCursor.Get();

        if (focusChanged) {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::FocusLock>>();
            auto &focusLock = lock.Get<ecs::FocusLock>();
            if (MenuOpen()) {
                focusLock.AcquireFocus(ecs::FocusLayer::Menu);
            } else {
                focusLock.ReleaseFocus(ecs::FocusLayer::Menu);
            }
        }
    }

    static bool StringVectorGetter(void *data, int idx, const char **out_text) {
        auto vec = (vector<string> *)data;
        if (out_text) {
            *out_text = vec->at(idx).c_str();
        }
        return true;
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
        ZoneScoped;
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
        PushFont(Font::Monospace, 25);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_AlwaysAutoResize;

        if (!logoTex) logoTex = graphics.context->LoadTexture(Assets().LoadImage("logos/sp-menu.png")->Get());
        ImVec2 logoSize(logoTex->GetWidth() * 0.5f, logoTex->GetHeight() * 0.5f);

        ImGui::SetNextWindowSizeConstraints(ImVec2(-1.0f, -1.0f), ImVec2(io.DisplaySize.x, io.DisplaySize.y));
        if (selectedScreen == MenuScreen::Main) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                ImGuiCond_Always,
                ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuMain", nullptr, flags);

            ImGui::Image((void *)logoTex->GetHandle(), logoSize);

            if (ImGui::Button("Resume")) {
                CVarMenuOpen.Set(false);
            }

            if (ImGui::Button("Save Game")) {
                GetConsoleManager().QueueParseAndExecute("savegame");
                // TODO: Add some sort of notifier that the game has been saved successfully
            }

            if (ImGui::Button("Load Game")) {
                selectedScreen = MenuScreen::SaveSelect;

                RefreshSaveList();
            }

            if (ImGui::Button("Scene Select")) {
                selectedScreen = MenuScreen::SceneSelect;

                RefreshSceneList();
            }

            if (ImGui::Button("Options")) {
                selectedScreen = MenuScreen::Options;
            }

            if (ImGui::Button("Quit")) {
                GetConsoleManager().QueueParseAndExecute("exit");
            }

            ImGui::End();
        } else if (selectedScreen == MenuScreen::SceneSelect) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                ImGuiCond_Always,
                ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuSceneSelect", nullptr, flags);

            ImGui::Image((void *)logoTex->GetHandle(), logoSize);

            PushFont(Font::Monospace, 32);

            ImGui::Text("Scene Select");
            ImGui::Text(" ");

            ImGui::PopFont();
            PushFont(Font::Monospace, 25);

            for (auto &[name, file] : sceneList) {
                if (ImGui::Button(name.c_str())) {
                    CVarMenuOpen.Set(false);
                    selectedScreen = MenuScreen::Main;
                    GetConsoleManager().QueueParseAndExecute("loadscene " + file);
                }
            }

            ImGui::PopFont();
            ImGui::Text(" ");

            if (ImGui::Button("Back")) {
                selectedScreen = MenuScreen::Main;
            }

            ImGui::End();
        } else if (selectedScreen == MenuScreen::SaveSelect) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                ImGuiCond_Always,
                ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuSaveSelect", nullptr, flags);

            ImGui::Image((void *)logoTex->GetHandle(), logoSize);

            PushFont(Font::Monospace, 32);

            ImGui::Text("Load Game");
            ImGui::Text(" ");

            ImGui::PopFont();
            PushFont(Font::Monospace, 25);

            for (auto &[name, file] : saveList) {
                if (ImGui::Button(name.c_str())) {
                    CVarMenuOpen.Set(false);
                    selectedScreen = MenuScreen::Main;
                    GetConsoleManager().QueueParseAndExecute("loadgame " + file);
                }
            }

            ImGui::PopFont();
            ImGui::Text(" ");

            if (ImGui::Button("Back")) {
                selectedScreen = MenuScreen::Main;
            }

            ImGui::End();
        } else if (selectedScreen == MenuScreen::Options) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                ImGuiCond_Always,
                ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuOptions", nullptr, flags);

            ImGui::Image((void *)logoTex->GetHandle(), logoSize);

            PushFont(Font::Monospace, 32);

            ImGui::Text("Options");
            ImGui::Text(" ");
            ImGui::Columns(2, "optcols", false);

            ImGui::PopFont();
            PushFont(Font::Monospace, 25);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 15));

            ImGui::Text("Resolution");
            ImGui::Text("Full Screen");
            ImGui::Text("Mirror VR View");

            ImGui::PopStyleVar();
            ImGui::NextColumn();
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0);

            {
                auto modes = graphics.context->MonitorModes();
                auto size = CVarWindowSize.Get();
                // If the current mode isn't in the list, add it to the bottom.
                int resIndex = std::find(modes.begin(), modes.end(), size) - modes.begin();
                if (resIndex < 0 || resIndex >= (int)modes.size()) {
                    resIndex = modes.size();
                    modes.push_back(size);
                }

                vector<string> resLabels = MakeResolutionLabels(modes);

                ImGui::PushItemWidth(300.0f);
                if (ImGui::Combo("##respicker", &resIndex, StringVectorGetter, &resLabels, modes.size())) {
                    CVarWindowSize.Set(modes[resIndex]);
                }
                ImGui::PopItemWidth();

                bool fullscreen = CVarWindowFullscreen.Get();
                if (ImGui::Checkbox("##fullscreencheck", &fullscreen)) {
                    CVarWindowFullscreen.Set(fullscreen);
                }

                auto &mirrorXrCVar = GetConsoleManager().GetCVar<bool>("r.mirrorxr");
                bool mirrorXR = mirrorXrCVar.Get();
                if (ImGui::Checkbox("##mirrorxrcheck", &mirrorXR)) {
                    mirrorXrCVar.Set(mirrorXR);
                }
            }

            ImGui::PopStyleVar(2);
            ImGui::PopFont();
            ImGui::Columns(1);
            ImGui::Text(" ");

            if (ImGui::Button("Done")) {
                selectedScreen = MenuScreen::Main;
            }

            ImGui::End();
        }

        ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(8);
    }

    bool MenuGuiManager::MenuOpen() const {
        return CVarMenuOpen.Get();
    }

    void MenuGuiManager::RefreshSceneList() {
        static const std::vector<std::string> builtIn = {
            "01-outside",
            "02-mirrors",
            "03-dark",
            "04-symmetry",
            "sponza",
            "station-center",
            "cornell-box-1",
            "cornell-box-3",
            "test1",
        };
        sceneList = {
            {"01 - Outside", "01-outside"},
            {"02 - Mirrors", "02-mirrors"},
            {"03 - Dark", "03-dark"},
            {"04 - Symmetry", "04-symmetry"},
            {"Sponza", "sponza"},
            {"Station Center", "station-center"},
            {"Cornell Box", "cornell-box-1"},
            {"Cornell Box Mirror", "cornell-box-3"},
            {"Test 1", "test1"},
        };
        auto sceneAssets = Assets().ListBundledAssets("scenes/", ".json");
        for (auto &path : sceneAssets) {
            if (starts_with(path, "scenes/") && ends_with(path, ".json")) {
                std::string scene = path.substr(7, path.length() - 5 - 7);
                if (contains(builtIn, scene)) continue;
                std::string name = scene;
                bool firstChar = true;
                for (size_t i = 0; i < scene.length(); i++) {
                    if (scene[i] == '_' || scene[i] == '-' || scene[i] == ' ') {
                        name[i] = ' ';
                        firstChar = true;
                    } else if (firstChar) {
                        name[i] = std::toupper(scene[i]);
                        firstChar = false;
                    }
                }
                sceneList.emplace_back(name, scene);
            }
        }
    }

    template<typename TP>
    std::time_t to_time_t(TP tp) {
        using namespace std::chrono;
        auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now() + system_clock::now());
        return system_clock::to_time_t(sctp);
    }

    void MenuGuiManager::RefreshSaveList() {
        saveList.clear();
        int i = 0;
        while (std::filesystem::exists("./saves/save" + std::to_string(i) + ".json")) {
            auto last_write_time = std::filesystem::last_write_time("./saves/save" + std::to_string(i) + ".json");
            std::time_t time = to_time_t(last_write_time);
            std::tm *tm = std::localtime(&time);

            std::string timeStr(100, '\0');
            size_t n = std::strftime(timeStr.data(), timeStr.size(), "%F %X", tm);
            timeStr.resize(n);
            saveList.emplace_back("Save " + std::to_string(i) + ": " + timeStr, "save" + std::to_string(i));
            i++;
        }
    }
} // namespace sp
