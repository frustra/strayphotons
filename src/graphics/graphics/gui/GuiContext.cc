/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GuiContext.hh"

#include "ecs/EcsImpl.hh"
#include "game/SceneManager.hh"
#include "graphics/gui/ImGuiKeyCodes.hh"
// #include "graphics/gui/definitions/EntityPickerGui.hh"
// #include "graphics/gui/definitions/InspectorGui.hh"
// #include "graphics/gui/definitions/LobbyGui.hh"
// #include "graphics/gui/definitions/SignalDisplayGui.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

#include <imgui/imgui.h>

namespace sp {
    static std::array fontList = {
        GuiFontDef{GuiFont::Primary, "DroidSans-Regular.ttf", 16.0f},
        GuiFontDef{GuiFont::Primary, "DroidSans-Regular.ttf", 32.0f},
        GuiFontDef{GuiFont::Monospace, "3270SemiCondensed-Regular.ttf", 25.0f},
        GuiFontDef{GuiFont::Monospace, "3270SemiCondensed-Regular.ttf", 32.0f},
    };

    GuiContext::GuiContext(const std::string &name) : name(name), guiEntity(ecs::Name("gui", name)) {
        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "gui",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto ent = scene->NewSystemEntity(lock, scene, guiEntity.Name());
                ent.Set<ecs::EventInput>(lock);
            });

        {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::EventInput>>();
            auto gui = guiEntity.Get(lock);
            Assertf(gui.Has<ecs::EventInput>(lock),
                "System Gui entity has no EventInput: %s",
                guiEntity.Name().String());
            auto &eventInput = gui.Get<ecs::EventInput>(lock);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_SCROLL);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_CURSOR);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_PRIMARY_TRIGGER);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_SECONDARY_TRIGGER);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_TEXT_INPUT);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_KEY_DOWN);
            eventInput.Register(lock, events, INPUT_EVENT_MENU_KEY_UP);
            eventInput.Register(lock, events, INTERACT_EVENT_INTERACT_POINT);
            eventInput.Register(lock, events, INTERACT_EVENT_INTERACT_PRESS);
        }

        imCtx = ImGui::CreateContext();
        SetGuiContext();
    }

    GuiContext::~GuiContext() {
        SetGuiContext();
        ImGui::DestroyContext(imCtx);
        imCtx = nullptr;

        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "gui",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                // TODO: Unregister from event input (or clean up weak ptrs automatically?)
                scene->RemoveEntity(lock, guiEntity.Get(lock));
            });
    }

    bool GuiContext::SetGuiContext() {
        ImGui::SetCurrentContext(imCtx);
        return true;
    }

    void GuiContext::BeforeFrame() {
        ZoneScoped;
        SetGuiContext();
        ImGuiIO &io = ImGui::GetIO();

        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::EventInput, ecs::TransformSnapshot>>();

            auto ent = guiEntity.Get(lock);
            ecs::Transform screenInverseTransform;
            if (ent.Has<ecs::TransformSnapshot>(lock)) {
                screenInverseTransform = ent.Get<ecs::TransformSnapshot>(lock).globalPose.GetInverse();
            }

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, events, event)) {
                if (event.name == INPUT_EVENT_MENU_SCROLL) {
                    if (event.data.type != ecs::EventDataType::Vec2) {
                        Warnf("System GUI received unexpected event data: %s, expected vec2", event.ToString());
                        continue;
                    }
                    auto &scroll = event.data.vec2;
                    io.AddMouseWheelEvent(scroll.x, scroll.y);
                } else if (event.name == INPUT_EVENT_MENU_CURSOR) {
                    if (event.data.type != ecs::EventDataType::Vec2) {
                        Warnf("System GUI received unexpected event data: %s, expected vec2", event.ToString());
                        continue;
                    }
                    auto &pos = event.data.vec2;
                    io.AddMousePosEvent(pos.x / io.DisplayFramebufferScale.x, pos.y / io.DisplayFramebufferScale.y);
                } else if (event.name == INPUT_EVENT_MENU_PRIMARY_TRIGGER) {
                    if (event.data.type != ecs::EventDataType::Bool) {
                        Warnf("System GUI received unexpected event data: %s, expected bool", event.ToString());
                        continue;
                    }
                    auto &down = event.data.b;
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left, down);
                } else if (event.name == INPUT_EVENT_MENU_SECONDARY_TRIGGER) {
                    if (event.data.type != ecs::EventDataType::Bool) {
                        Warnf("System GUI received unexpected event data: %s, expected bool", event.ToString());
                        continue;
                    }
                    auto &down = event.data.b;
                    io.AddMouseButtonEvent(ImGuiMouseButton_Right, down);
                } else if (event.name == INPUT_EVENT_MENU_TEXT_INPUT) {
                    if (event.data.type != ecs::EventDataType::Uint) {
                        Warnf("System GUI received unexpected event data: %s, expected uint", event.ToString());
                        continue;
                    }
                    io.AddInputCharacter(event.data.ui);
                } else if (event.name == INPUT_EVENT_MENU_KEY_DOWN) {
                    if (event.data.type != ecs::EventDataType::Int) {
                        Warnf("System GUI received unexpected event data: %s, expected int", event.ToString());
                        continue;
                    }
                    KeyCode keyCode = (KeyCode)event.data.i;
                    if (keyCode == KEY_LEFT_CONTROL || keyCode == KEY_RIGHT_CONTROL) {
                        io.AddKeyEvent(ImGuiMod_Ctrl, true);
                    } else if (keyCode == KEY_LEFT_SHIFT || keyCode == KEY_RIGHT_SHIFT) {
                        io.AddKeyEvent(ImGuiMod_Shift, true);
                    } else if (keyCode == KEY_LEFT_ALT || keyCode == KEY_RIGHT_ALT) {
                        io.AddKeyEvent(ImGuiMod_Alt, true);
                    } else if (keyCode == KEY_LEFT_SUPER || keyCode == KEY_RIGHT_SUPER) {
                        io.AddKeyEvent(ImGuiMod_Super, true);
                    }
                    auto imguiKey = ImGuiKeyMapping.find(keyCode);
                    if (imguiKey != ImGuiKeyMapping.end()) {
                        io.AddKeyEvent(imguiKey->second, true);
                    }
                } else if (event.name == INPUT_EVENT_MENU_KEY_UP) {
                    if (event.data.type != ecs::EventDataType::Int) {
                        Warnf("System GUI received unexpected event data: %s, expected int", event.ToString());
                        continue;
                    }
                    KeyCode keyCode = (KeyCode)event.data.i;
                    if (keyCode == KEY_LEFT_CONTROL || keyCode == KEY_RIGHT_CONTROL) {
                        io.AddKeyEvent(ImGuiMod_Ctrl, false);
                    } else if (keyCode == KEY_LEFT_SHIFT || keyCode == KEY_RIGHT_SHIFT) {
                        io.AddKeyEvent(ImGuiMod_Shift, false);
                    } else if (keyCode == KEY_LEFT_ALT || keyCode == KEY_RIGHT_ALT) {
                        io.AddKeyEvent(ImGuiMod_Alt, false);
                    } else if (keyCode == KEY_LEFT_SUPER || keyCode == KEY_RIGHT_SUPER) {
                        io.AddKeyEvent(ImGuiMod_Super, false);
                    }
                    auto imguiKey = ImGuiKeyMapping.find(keyCode);
                    if (imguiKey != ImGuiKeyMapping.end()) {
                        io.AddKeyEvent(imguiKey->second, false);
                    }
                } else if (event.name == INTERACT_EVENT_INTERACT_POINT) {
                    auto existingState = std::find_if(pointingStack.begin(), pointingStack.end(), [&](auto &state) {
                        return state.sourceEntity == event.source;
                    });

                    if (event.data.type == ecs::EventDataType::Transform) {
                        auto &pointWorld = event.data.transform.GetPosition();
                        auto pointOnScreen = screenInverseTransform * glm::vec4(pointWorld, 1);
                        pointOnScreen += 0.5f;

                        glm::vec2 mousePos = {
                            pointOnScreen.x * io.DisplaySize.x,
                            (1.0f - pointOnScreen.y) * io.DisplaySize.y,
                        };

                        if (existingState != pointingStack.end()) {
                            existingState->mousePos = mousePos;
                        } else {
                            pointingStack.emplace_back(PointingState{event.source, mousePos});
                        }
                    } else if (event.data.type == ecs::EventDataType::Vec2) {
                        glm::vec2 &mousePos = event.data.vec2;
                        if (existingState != pointingStack.end()) {
                            existingState->mousePos = mousePos;
                        } else {
                            pointingStack.emplace_back(PointingState{event.source, mousePos});
                        }
                    } else if (event.data.type == ecs::EventDataType::Bool) {
                        if (existingState != pointingStack.end()) {
                            if (existingState->mouseDown) {
                                // Keep state if mouse was dragged off screen
                                existingState->mousePos = {-FLT_MAX, -FLT_MAX};
                            } else {
                                pointingStack.erase(existingState);
                            }
                        }
                    } else {
                        Warnf("World GUI received unexpected event data: %s, expected Transform, vec2, or bool",
                            event.ToString());
                    }
                } else if (event.name == INTERACT_EVENT_INTERACT_PRESS) {
                    auto existingState = std::find_if(pointingStack.begin(), pointingStack.end(), [&](auto &state) {
                        return state.sourceEntity == event.source;
                    });

                    if (event.data.type == ecs::EventDataType::Bool) {
                        bool mouseDown = event.data.b;
                        if (existingState != pointingStack.end()) {
                            if (mouseDown != existingState->mouseDown) {
                                // Send previous mouse event immediately so fast clicks aren't missed
                                io.AddMousePosEvent(existingState->mousePos.x, existingState->mousePos.y);
                                io.AddMouseButtonEvent(ImGuiMouseButton_Left, existingState->mouseDown);
                                existingState->mouseDown = mouseDown;
                            }
                            if (!mouseDown && existingState->mousePos == glm::vec2(-FLT_MAX, -FLT_MAX)) {
                                // Remove the state if the mouse is released off screen
                                pointingStack.erase(existingState);
                            }
                        } else {
                            Warnf("Entity %s sent press event to world gui %s without point event",
                                std::to_string(event.source),
                                guiEntity.Name().String());
                        }
                    } else {
                        Warnf("World GUI received unexpected event data: %s, expected bool", event.ToString());
                        continue;
                    }
                }
            }

            if (!pointingStack.empty()) {
                auto &state = pointingStack.back();
                io.AddMousePosEvent(state.mousePos.x, state.mousePos.y);
                io.AddMouseButtonEvent(ImGuiMouseButton_Left, state.mouseDown);
            } else {
                io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
                io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
            }
        }
    }

    // flatview#window -> overlay#render_output -> menu#render_output -> hud#render_output -> flatview#view
    //                  console, profiler_gui, fps       menu           inspector, entity_picker
    // render_output -> signal_display
    // texture override on model instead of screen

    // gui targets:
    // - system overlay / view overlay
    // - pause menu / view overlay
    // - game hud / view overlay
    // - in-game displays

    // All GuiElements are textures and unique entities
    // They can specify a background texture input
    // They have regular Transforms to put them in UI-space

    // Components:
    // render_output or render_output?: (generate a render graph output)
    // - source (view, render_output, asset)
    // - output_size (maybe inherit or auto)
    // - gui_elements (array of gui entities)
    // - effects? (blur, crosshair, other post-processing)
    // - sprites? (transform tree based positioning)
    // gui_element: (attachable to render_output entity + self by default)
    // - anchor (maybe move to gui_elements)
    // - preferred_size

    void GuiContext::DefineWindows() {
        ZoneScoped;
        SetGuiContext();

        std::sort(elements.begin(), elements.end(), [](const auto &lhs, const auto &rhs) {
            return lhs.anchor < rhs.anchor;
        });

        ImGuiViewport *imguiViewport = ImGui::GetMainViewport();
        Assertf(imguiViewport, "ImGui::GetMainViewport() returned null");
        ImVec2 viewportPos = imguiViewport->WorkPos;
        ImVec2 viewportSize = imguiViewport->WorkSize;
        for (auto &element : elements) {
            if (!element.definition) continue;

            ecs::Entity ent = guiEntity.GetLive();

            if (element.definition->PreDefine(ent)) {
                ImVec2 windowSize(element.preferredSize.x, element.preferredSize.y);
                if (element.anchor != ecs::GuiLayoutAnchor::Floating) {
                    windowSize.x = std::min(windowSize.x, std::min(viewportSize.x, imguiViewport->WorkSize.x * 0.4f));
                    windowSize.y = std::min(windowSize.y, std::min(viewportSize.y, imguiViewport->WorkSize.y * 0.4f));
                    if (windowSize.x <= 0) windowSize.x = viewportSize.x;
                    if (windowSize.y <= 0) windowSize.y = viewportSize.y;
                    ImGui::SetNextWindowSize(windowSize);
                }

                switch (element.anchor) {
                case ecs::GuiLayoutAnchor::Fullscreen:
                    ImGui::SetNextWindowPos(viewportPos);
                    break;
                case ecs::GuiLayoutAnchor::Left:
                    ImGui::SetNextWindowPos(viewportPos);
                    viewportPos.x += windowSize.x;
                    viewportSize.x -= windowSize.x;
                    break;
                case ecs::GuiLayoutAnchor::Top:
                    ImGui::SetNextWindowPos(viewportPos);
                    viewportPos.y += windowSize.y;
                    viewportSize.y -= windowSize.y;
                    break;
                case ecs::GuiLayoutAnchor::Right:
                    ImGui::SetNextWindowPos(ImVec2(viewportPos.x + viewportSize.x, viewportPos.y),
                        ImGuiCond_None,
                        ImVec2(1, 0));
                    viewportSize.x -= windowSize.x;
                    break;
                case ecs::GuiLayoutAnchor::Bottom:
                    ImGui::SetNextWindowPos(ImVec2(viewportPos.x, viewportPos.y + viewportSize.y),
                        ImGuiCond_None,
                        ImVec2(0, 1));
                    viewportSize.y -= windowSize.y;
                    break;
                case ecs::GuiLayoutAnchor::Floating:
                    // Noop
                    break;
                default:
                    Abortf("Unexpected GuiLayoutAnchor: %s", element.anchor);
                }

                ImGui::Begin(element.definition->name.c_str(), nullptr, element.definition->windowFlags);
                element.definition->DefineContents(ent);
                ImGui::End();
                element.definition->PostDefine(ent);
            }
        }
    }

    ImDrawData *GuiContext::GetDrawData(glm::vec2, glm::vec2, float) const {
        return ImGui::GetDrawData();
    }

    void GuiContext::Attach(ecs::Entity guiElementEntity, ecs::GuiLayoutAnchor anchor, glm::ivec2 preferredSize) {
        auto it = std::find_if(elements.begin(), elements.end(), [&](auto &info) {
            return info.ent == guiElementEntity;
        });
        if (it == elements.end()) elements.emplace_back(guiElementEntity, anchor, preferredSize);
    }

    void GuiContext::Detach(ecs::Entity guiElementEntity) {
        auto it = std::find_if(elements.begin(), elements.end(), [&](auto &info) {
            return info.ent == guiElementEntity;
        });
        if (it != elements.end()) elements.erase(it);
    }

    void GuiContext::PushFont(GuiFont fontType, float fontSize) {
        auto &io = ImGui::GetIO();
        Assert(io.Fonts->Fonts.size() == fontList.size() + 1, "unexpected font list size");

        for (size_t i = 0; i < fontList.size(); i++) {
            auto &f = fontList[i];
            if (f.type == fontType && f.size == fontSize) {
                ImGui::PushFont(io.Fonts->Fonts[i + 1]);
                return;
            }
        }

        Abortf("missing font type %d with size %f", (int)fontType, fontSize);
    }

    // ecs::EntityRef LookupInternalGui(const std::string &windowName) {
    //     if (windowName == "lobby") {
    //         static const auto lobby = make_shared<LobbyGui>(windowName);
    //         return lobby;
    //     } else if (windowName == "entity_picker") {
    //         static const auto entityPicker = make_shared<EntityPickerGui>(windowName);
    //         return entityPicker;
    //     } else if (windowName == "inspector") {
    //         static const auto inspector = make_shared<InspectorGui>(windowName);
    //         return inspector;
    //         // } else if (windowName == "signal_display") {
    //         //     static const auto signalDisplay = make_shared<SignalDisplayGui>(windowName);
    //         //     return signalDisplay;
    //     }
    //     return std::weak_ptr<ecs::GuiElement>();
    // }

    // std::shared_ptr<ecs::ScriptState> LookupScriptGui(const std::string &windowName, const ecs::Scripts *scripts) {
    //     if (windowName.empty() && scripts) {
    //         for (auto &script : scripts->scripts) {
    //             if (!script.state) continue;
    //             ecs::ScriptState &state = *script.state;
    //             if (state.definition.type == ecs::ScriptType::GuiScript) {
    //                 if (std::holds_alternative<ecs::GuiRenderFuncs>(state.definition.callback)) {
    //                     return script.state;
    //                 } else {
    //                     Errorf("Gui script %s has invalid callback type: GuiScript != GuiRender",
    //                         state.definition.name);
    //                 }
    //             }
    //         }
    //     }
    //     if (!windowName.empty()) Errorf("unknown gui window: %s", windowName);
    //     return nullptr;
    // }

    std::span<GuiFontDef> GetGuiFontList() {
        return fontList;
    }
} // namespace sp
