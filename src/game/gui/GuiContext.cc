/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GuiContext.hh"

#include "ecs/EcsImpl.hh"
#include "game/SceneManager.hh"
#include "graphics/GenericCompositor.hh"
#include "gui/GuiDrawData.hh"
#include "gui/ImGuiHelpers.hh"
#include "gui/ImGuiKeyCodes.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

#include <imgui.h>

namespace sp {
    static std::array fontList = {
        GuiFontDef{GuiFont::Primary, "DroidSans-Regular.ttf", 16.0f},
        GuiFontDef{GuiFont::Primary, "DroidSans-Regular.ttf", 32.0f},
        GuiFontDef{GuiFont::Monospace, "3270SemiCondensed-Regular.ttf", 25.0f},
        GuiFontDef{GuiFont::Monospace, "3270SemiCondensed-Regular.ttf", 32.0f},
    };

    GuiContext::GuiContext(const ecs::EntityRef &guiEntity) : guiEntity(guiEntity) {
        if (guiEntity) {
            ecs::QueueTransaction<ecs::Write<ecs::EventInput>>(
                [guiEntity = this->guiEntity, events = this->events](auto &lock) {
                    ecs::Entity ent = guiEntity.Get(lock);
                    Assertf(ent.Has<ecs::EventInput>(lock),
                        "GuiContext entity has no EventInput: %s",
                        guiEntity.Name().String());
                    auto &eventInput = ent.Get<ecs::EventInput>(lock);
                    eventInput.Register(lock, events, INPUT_EVENT_MENU_SCROLL);
                    eventInput.Register(lock, events, INPUT_EVENT_MENU_CURSOR);
                    eventInput.Register(lock, events, INPUT_EVENT_MENU_PRIMARY_TRIGGER);
                    eventInput.Register(lock, events, INPUT_EVENT_MENU_SECONDARY_TRIGGER);
                    eventInput.Register(lock, events, INPUT_EVENT_MENU_TEXT_INPUT);
                    eventInput.Register(lock, events, INPUT_EVENT_MENU_KEY_DOWN);
                    eventInput.Register(lock, events, INPUT_EVENT_MENU_KEY_UP);
                    eventInput.Register(lock, events, INTERACT_EVENT_INTERACT_POINT);
                    eventInput.Register(lock, events, INTERACT_EVENT_INTERACT_PRESS);
                });
        }

        imCtx = ImGui::CreateContext();
    }

    GuiContext::GuiContext(GuiContext &&other) {
        imCtx = std::move(other.imCtx);
        guiEntity = std::move(other.guiEntity);
        events = std::move(other.events);
        elements = std::move(other.elements);
        pointingStack = std::move(other.pointingStack);
    }

    GuiContext::~GuiContext() {
        ImGui::DestroyContext(imCtx);
        imCtx = nullptr;

        if (guiEntity) {
            ecs::QueueTransaction<ecs::Write<ecs::EventInput>>(
                [guiEntity = this->guiEntity, events = this->events](auto &lock) {
                    ecs::Entity ent = guiEntity.Get(lock);
                    if (ent.Has<ecs::EventInput>(lock)) {
                        auto &eventInput = ent.Get<ecs::EventInput>(lock);
                        eventInput.Unregister(events, INPUT_EVENT_MENU_SCROLL);
                        eventInput.Unregister(events, INPUT_EVENT_MENU_CURSOR);
                        eventInput.Unregister(events, INPUT_EVENT_MENU_PRIMARY_TRIGGER);
                        eventInput.Unregister(events, INPUT_EVENT_MENU_SECONDARY_TRIGGER);
                        eventInput.Unregister(events, INPUT_EVENT_MENU_TEXT_INPUT);
                        eventInput.Unregister(events, INPUT_EVENT_MENU_KEY_DOWN);
                        eventInput.Unregister(events, INPUT_EVENT_MENU_KEY_UP);
                        eventInput.Unregister(events, INTERACT_EVENT_INTERACT_POINT);
                        eventInput.Unregister(events, INTERACT_EVENT_INTERACT_PRESS);
                    }
                });
        }
    }

    bool GuiContext::SetGuiContext() {
        ImGui::SetCurrentContext(imCtx);
        return true;
    }

    bool GuiContext::BeforeFrame(GenericCompositor &compositor) {
        ZoneScoped;
        SetGuiContext();

        ImGui::StyleColorsClassic();

        ImGuiIO &io = ImGui::GetIO();
        io.MouseDrawCursor = false;

        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::EventInput, ecs::TransformSnapshot>>();

            ecs::Entity ent = guiEntity.Get(lock);
            ecs::Transform screenInverseTransform;
            if (ent.Has<ecs::TransformSnapshot>(lock)) {
                screenInverseTransform = ent.Get<ecs::TransformSnapshot>(lock).globalPose.GetInverse();
            }

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, events, event)) {
                auto existingState = std::find_if(pointingStack.begin(), pointingStack.end(), [&](auto &state) {
                    return state.sourceEntity == event.source;
                });

                if (event.name == INPUT_EVENT_MENU_SCROLL) {
                    if (event.data.type != ecs::EventDataType::Vec2) {
                        Warnf("GuiContext received unexpected event data: %s, expected vec2", event.ToString());
                        continue;
                    }
                    auto &scroll = event.data.vec2;
                    io.AddMousePosEvent(existingState->mousePos.x, existingState->mousePos.y);
                    io.AddMouseWheelEvent(scroll.x, scroll.y);
                } else if (event.name == INPUT_EVENT_MENU_CURSOR) {
                    if (event.data.type == ecs::EventDataType::Vec2) {
                        glm::vec2 &mousePos = event.data.vec2;
                        mousePos.x /= io.DisplayFramebufferScale.x;
                        mousePos.y /= io.DisplayFramebufferScale.y;
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
                        Warnf("GuiContext received unexpected event data: %s, expected vec2", event.ToString());
                    }
                } else if (event.name == INPUT_EVENT_MENU_PRIMARY_TRIGGER) {
                    if (event.data.type != ecs::EventDataType::Bool) {
                        Warnf("GuiContext received unexpected event data: %s, expected bool", event.ToString());
                        continue;
                    }
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
                        Warnf("Entity %s sent primary trigger event to gui %s without cursor event",
                            std::to_string(event.source),
                            guiEntity.Name().String());
                    }
                } else if (event.name == INPUT_EVENT_MENU_SECONDARY_TRIGGER) {
                    if (event.data.type != ecs::EventDataType::Bool) {
                        Warnf("GuiContext received unexpected event data: %s, expected bool", event.ToString());
                        continue;
                    }
                    auto &down = event.data.b;
                    io.AddMousePosEvent(existingState->mousePos.x, existingState->mousePos.y);
                    io.AddMouseButtonEvent(ImGuiMouseButton_Right, down);
                } else if (event.name == INPUT_EVENT_MENU_TEXT_INPUT) {
                    if (event.data.type != ecs::EventDataType::Uint) {
                        Warnf("GuiContext received unexpected event data: %s, expected uint", event.ToString());
                        continue;
                    }
                    io.AddInputCharacter(event.data.ui);
                } else if (event.name == INPUT_EVENT_MENU_KEY_DOWN) {
                    if (event.data.type != ecs::EventDataType::Int) {
                        Warnf("GuiContext received unexpected event data: %s, expected int", event.ToString());
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
                        Warnf("GuiContext received unexpected event data: %s, expected int", event.ToString());
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
                            Warnf("Entity %s sent press event to gui %s without point event",
                                std::to_string(event.source),
                                guiEntity.Name().String());
                        }
                    } else {
                        Warnf("GuiContext received unexpected event data: %s, expected bool", event.ToString());
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

        bool anyEnabled = false;
        for (auto &element : elements) {
            if (!element.definition) continue;
            ecs::Entity ent = guiEntity.GetLive();
            if (!ent) continue;
            element.enabled = element.definition->BeforeFrame(compositor, ent);
            anyEnabled |= element.enabled;
        }
        return anyEnabled;
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
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

        std::sort(elements.begin(), elements.end(), [](const auto &lhs, const auto &rhs) {
            return lhs.anchor < rhs.anchor;
        });

        ImGuiViewport *imguiViewport = ImGui::GetMainViewport();
        Assertf(imguiViewport, "ImGui::GetMainViewport() returned null");
        ImVec2 viewportPos = imguiViewport->WorkPos;
        ImVec2 viewportSize = imguiViewport->WorkSize;
        for (auto &element : elements) {
            if (!element.definition || !element.enabled) continue;

            ecs::Entity ent = guiEntity.GetLive();
            if (!ent) continue;

            element.definition->PreDefine(ent);

            ImVec2 windowSize(element.preferredSize.x, element.preferredSize.y);
            if (element.anchor != ecs::GuiLayoutAnchor::Floating) {
                windowSize.x = std::min(windowSize.x, viewportSize.x);
                windowSize.y = std::min(windowSize.y, viewportSize.y);
                if (windowSize.x < 0) {
                    windowSize.x = imguiViewport->WorkSize.x * std::clamp(windowSize.x / -100.0f, 0.0f, 1.0f);
                }
                if (windowSize.y < 0) {
                    windowSize.y = imguiViewport->WorkSize.y * std::clamp(windowSize.y / -100.0f, 0.0f, 1.0f);
                }
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

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    void GuiContext::GetDrawData(GuiDrawData &output) {
        ConvertImDrawData(ImGui::GetDrawData(), &output);
    }

    void GuiContext::ClearEntities() {
        erase_if(elements, [](auto &info) {
            return info.ent;
        });
    }

    void GuiContext::AddEntity(ecs::Entity guiElementEntity,
        std::shared_ptr<ecs::GuiDefinition> definition,
        ecs::GuiLayoutAnchor anchor,
        glm::ivec2 preferredSize) {
        Assertf(guiElementEntity, "GuiContext::AddEntity called with null entity");
        Assertf(definition, "GuiContext::AddEntity called with null definition");
        elements.emplace_back(guiElementEntity, anchor, preferredSize, definition, true);
    }

    void GuiContext::Attach(std::shared_ptr<ecs::GuiDefinition> definition,
        ecs::GuiLayoutAnchor anchor,
        glm::ivec2 preferredSize) {
        auto it = std::find_if(elements.begin(), elements.end(), [&](auto &info) {
            return info.definition == definition;
        });
        if (it == elements.end()) elements.emplace_back(ecs::Entity(), anchor, preferredSize, definition, true);
    }

    void GuiContext::Detach(std::shared_ptr<ecs::GuiDefinition> definition) {
        auto it = std::find_if(elements.begin(), elements.end(), [&](auto &info) {
            return info.definition == definition;
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

    std::span<GuiFontDef> GetGuiFontList() {
        return fontList;
    }
} // namespace sp
