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

    MenuGuiManager::MenuGuiManager(GraphicsManager &graphics) : FlatViewGuiContext("menu"), graphics(graphics) {
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
        FlatViewGuiContext::BeforeFrame();

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
        PushFont(GuiFont::Monospace, 25);

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

            static int textureIndex = 0;
            ImGui::SliderInt("Texture Index", &textureIndex, -1, 4096);
            ImGui::Image((ImTextureID)(int64_t)textureIndex, logoSize);
            // ImGui::Image((ImTextureID)logoTex->GetHandle(), logoSize);

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

            ImGui::Image((ImTextureID)logoTex->GetHandle(), logoSize);

            PushFont(GuiFont::Monospace, 32);

            ImGui::TextUnformatted("Scene Select");
            ImGui::TextUnformatted(" ");

            ImGui::PopFont();
            PushFont(GuiFont::Monospace, 25);

            for (auto &[name, file] : sceneList) {
                if (ImGui::Button(name.c_str())) {
                    CVarMenuOpen.Set(false);
                    selectedScreen = MenuScreen::Main;
                    GetConsoleManager().QueueParseAndExecute("loadscene " + file);
                }
            }

            ImGui::PopFont();
            ImGui::TextUnformatted(" ");

            if (ImGui::Button("Back")) {
                selectedScreen = MenuScreen::Main;
            }

            ImGui::End();
        } else if (selectedScreen == MenuScreen::SaveSelect) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                ImGuiCond_Always,
                ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuSaveSelect", nullptr, flags);

            ImGui::Image((ImTextureID)logoTex->GetHandle(), logoSize);

            PushFont(GuiFont::Monospace, 32);

            ImGui::TextUnformatted("Load Game");
            ImGui::TextUnformatted(" ");

            ImGui::PopFont();
            PushFont(GuiFont::Monospace, 25);

            for (auto &[name, file] : saveList) {
                if (ImGui::Button(name.c_str())) {
                    CVarMenuOpen.Set(false);
                    selectedScreen = MenuScreen::Main;
                    GetConsoleManager().QueueParseAndExecute("loadgame " + file);
                }
            }

            ImGui::PopFont();
            ImGui::TextUnformatted(" ");

            if (ImGui::Button("Back")) {
                selectedScreen = MenuScreen::Main;
            }

            ImGui::End();
        } else if (selectedScreen == MenuScreen::Options) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                ImGuiCond_Always,
                ImVec2(0.5f, 0.5f));
            ImGui::Begin("MenuOptions", nullptr, flags);

            ImGui::Image((ImTextureID)logoTex->GetHandle(), logoSize);

            PushFont(GuiFont::Monospace, 32);

            ImGui::TextUnformatted("Options");
            ImGui::TextUnformatted(" ");
            ImGui::Columns(2, "optcols", false);

            ImGui::PopFont();
            PushFont(GuiFont::Monospace, 25);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 15));

            ImGui::TextUnformatted("Resolution");
            ImGui::TextUnformatted("Full Screen");
            ImGui::TextUnformatted("Show FPS");
            ImGui::TextUnformatted("Field of View");
            ImGui::TextUnformatted("UI Scaling");
            ImGui::TextUnformatted("Mirror VR View");
            ImGui::TextUnformatted("Voxel Lighting Mode");
            ImGui::TextUnformatted("Voxel Traced Reflections");
            ImGui::TextUnformatted("Shadow Quality");

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

                auto &showFpsCVar = GetConsoleManager().GetCVar<bool>("r.showfps");
                bool showFps = showFpsCVar.Get();
                if (ImGui::Checkbox("##showfpscheck", &showFps)) {
                    showFpsCVar.Set(showFps);
                }

                auto &fovCVar = GetConsoleManager().GetCVar<float>("r.fieldofview");
                float fovDegrees = fovCVar.Get();
                ImGui::PushItemWidth(300.0f);
                if (ImGui::SliderFloat("##fovDegrees", &fovDegrees, 1.0f, 160.0f, "%.0f degrees")) {
                    fovCVar.Set(fovDegrees);
                }

                float scale = CVarWindowScale.Get().x * 100.0f;
                ImGui::PushItemWidth(300.0f);
                if (ImGui::InputFloat("##uiscaleinput", &scale, 5.0f, 10.0f, "%.0f%%")) {
                    if (scale >= 5.0f) {
                        CVarWindowScale.Set(glm::vec2(scale / 100.0f));
                    }
                }
                ImGui::PopItemWidth();

                auto &mirrorXrCVar = GetConsoleManager().GetCVar<bool>("r.mirrorxr");
                bool mirrorXR = mirrorXrCVar.Get();
                if (ImGui::Checkbox("##mirrorxrcheck", &mirrorXR)) {
                    mirrorXrCVar.Set(mirrorXR);
                }

                static const std::array<std::pair<const char *, int>, 5> voxelLightingModes = {
                    std::make_pair("Full Lighting", 1),
                    std::make_pair("Off", 0),
                    std::make_pair("[Debug] Indirect Only", 2),
                    std::make_pair("[Debug] Diffuse Only", 3),
                    std::make_pair("[Debug] Specular Only", 4),
                };

                auto &lightingModeCVar = GetConsoleManager().GetCVar<int>("r.lightingmode");
                int voxelLighting = lightingModeCVar.Get();
                int voxelLightingIndex = 0;
                for (int i = 0; i < voxelLightingModes.size(); i++) {
                    if (voxelLightingModes[i].second == voxelLighting) {
                        voxelLightingIndex = i;
                        break;
                    }
                }
                ImGui::PushItemWidth(300.0f);
                if (ImGui::Combo(
                        "##bouncelightingcheck",
                        &voxelLightingIndex,
                        [](void *, int i) {
                            return voxelLightingModes.at(i).first;
                        },
                        nullptr,
                        voxelLightingModes.size())) {
                    lightingModeCVar.Set(voxelLightingModes.at(voxelLightingIndex).second);
                }
                ImGui::PopItemWidth();

                auto &specularTracingCVar = GetConsoleManager().GetCVar<bool>("r.speculartracing");
                bool specularTracing = specularTracingCVar.Get();
                if (ImGui::Checkbox("##tracedreflectionscheck", &specularTracing)) {
                    specularTracingCVar.Set(specularTracing);
                }

                struct ShadowSetting {
                    const char *name;
                    int sizeOffset;
                    int sampleCount;
                    float sampleWidth;
                };

                static const std::array<ShadowSetting, 5> shadowResolutions = {
                    ShadowSetting{"Overdrive", 1, 12, 3.5f},
                    ShadowSetting{"Very High", 0, 11, 3.8f},
                    ShadowSetting{"High", -1, 9, 3.2f},
                    ShadowSetting{"Medium", -2, 8, 2.4f},
                    ShadowSetting{"Low", -3, 7, 1.9f},
                };

                auto &shadowMapSizeOffsetCVar = GetConsoleManager().GetCVar<int>("r.shadowmapsizeoffset");
                auto &shadowMapSampleCountCVar = GetConsoleManager().GetCVar<int>("r.shadowmapsamplecount");
                auto &shadowMapSampleWidthCVar = GetConsoleManager().GetCVar<float>("r.shadowmapsamplewidth");
                int shadowMapSizeOffset = shadowMapSizeOffsetCVar.Get();
                int shadowMapSampleCount = shadowMapSampleCountCVar.Get();
                float shadowMapSampleWidth = shadowMapSampleWidthCVar.Get();
                int shadowResolutionModeIndex = shadowResolutions.size();
                for (int i = 0; i < shadowResolutions.size(); i++) {
                    if (shadowResolutions[i].sizeOffset == shadowMapSizeOffset) {
                        shadowResolutionModeIndex = i;
                        break;
                    }
                }
                if (shadowResolutionModeIndex < shadowResolutions.size()) {
                    auto &shadowSetting = shadowResolutions[shadowResolutionModeIndex];
                    if (shadowSetting.sampleCount != shadowMapSampleCount ||
                        shadowSetting.sampleWidth != shadowMapSampleWidth) {
                        shadowResolutionModeIndex = shadowResolutions.size();
                    }
                }
                int itemCount = std::max((int)shadowResolutions.size(), shadowResolutionModeIndex + 1);
                ImGui::PushItemWidth(300.0f);
                if (ImGui::Combo(
                        "##shadowqualitypicker",
                        &shadowResolutionModeIndex,
                        [](void *, int i) {
                            if (i < 0 || i >= shadowResolutions.size()) return "Custom";
                            return shadowResolutions[i].name;
                        },
                        nullptr,
                        itemCount)) {
                    if (shadowResolutionModeIndex >= 0 && shadowResolutionModeIndex < shadowResolutions.size()) {
                        auto &shadowSetting = shadowResolutions[shadowResolutionModeIndex];
                        shadowMapSizeOffsetCVar.Set(shadowSetting.sizeOffset);
                        shadowMapSampleCountCVar.Set(shadowSetting.sampleCount);
                        shadowMapSampleWidthCVar.Set(shadowSetting.sampleWidth);
                    }
                }
                ImGui::PopItemWidth();
            }

            ImGui::PopStyleVar(2);
            ImGui::PopFont();
            ImGui::Columns(1);
            ImGui::TextUnformatted(" ");

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
        auto sceneAssets = Assets().ListBundledAssets("scenes/", ".json", 0);
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
