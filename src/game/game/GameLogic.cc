/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GameLogic.hh"

#include "console/Console.hh"
#include "core/LockFreeEventQueue.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptManager.hh"
#include "input/BindingNames.hh"

namespace sp {
    GameLogic::GameLogic(LockFreeEventQueue<ecs::Event> &windowInputQueue, bool stepMode)
        : RegisteredThread("GameLogic", 120.0, true), windowInputQueue(windowInputQueue), stepMode(stepMode) {
        if (stepMode) {
            funcs.Register<unsigned int>("steplogic",
                "Advance the game logic by N frames, default is 1",
                [this](unsigned int arg) {
                    this->Step(std::max(1u, arg));
                });
        }
    }

    void GameLogic::StartThread() {
        RegisteredThread::StartThread(stepMode);
    }

    void GameLogic::UpdateInputEvents(const ecs::Lock<ecs::SendEventsLock, ecs::Write<ecs::Signals>> &lock,
        LockFreeEventQueue<ecs::Event> &inputQueue) {
        static const ecs::EntityRef keyboardEntity = ecs::Name("input", "keyboard");
        static const ecs::EntityRef mouseEntity = ecs::Name("input", "mouse");

        static glm::vec2 prevMousePos = {};

        auto keyboard = keyboardEntity.Get(lock);
        auto mouse = mouseEntity.Get(lock);

        // ecs::Event event;
        // while (inputQueue.Poll(event, lock.GetTransactionId())) {
        inputQueue.PollEvents([&](const ecs::Event &event) {
            if (event.source == keyboard) {
                ecs::EventBindings::SendEvent(lock, keyboardEntity, event);
            } else if (event.source == mouse) {
                ecs::EventBindings::SendEvent(lock, mouseEntity, event);
            }

            if (event.name == INPUT_EVENT_KEYBOARD_KEY_DOWN) {
                auto &keyName = std::get<std::string>(event.data);
                std::string eventName = INPUT_EVENT_KEYBOARD_KEY_BASE + keyName;
                ecs::EventBindings::SendEvent(lock, keyboardEntity, ecs::Event{eventName, keyboard, true});

                ecs::SignalRef signalRef(keyboard, INPUT_SIGNAL_KEYBOARD_KEY_BASE + keyName);
                signalRef.SetValue(lock, 1.0);
            } else if (event.name == INPUT_EVENT_KEYBOARD_KEY_UP) {
                auto &keyName = std::get<std::string>(event.data);
                std::string eventName = INPUT_EVENT_KEYBOARD_KEY_BASE + keyName;
                ecs::EventBindings::SendEvent(lock, keyboardEntity, ecs::Event{eventName, keyboard, false});

                ecs::SignalRef signalRef(keyboard, INPUT_SIGNAL_KEYBOARD_KEY_BASE + keyName);
                signalRef.ClearValue(lock);
            } else if (event.name == INPUT_EVENT_MOUSE_POSITION) {
                auto &mousePos = std::get<glm::vec2>(event.data);
                ecs::EventBindings::SendEvent(lock,
                    mouseEntity,
                    ecs::Event{INPUT_EVENT_MOUSE_MOVE, mouse, mousePos - prevMousePos});
                prevMousePos = mousePos;

                ecs::SignalRef refX(mouse, INPUT_SIGNAL_MOUSE_CURSOR_X);
                ecs::SignalRef refY(mouse, INPUT_SIGNAL_MOUSE_CURSOR_Y);
                refX.SetValue(lock, mousePos.x);
                refY.SetValue(lock, mousePos.y);
            } else if (event.name == INPUT_EVENT_MOUSE_LEFT_CLICK) {
                ecs::SignalRef signalRef(mouse, INPUT_SIGNAL_MOUSE_BUTTON_LEFT);
                if (std::get<bool>(event.data)) {
                    signalRef.SetValue(lock, 1.0);
                } else {
                    signalRef.ClearValue(lock);
                }
            } else if (event.name == INPUT_EVENT_MOUSE_MIDDLE_CLICK) {
                ecs::SignalRef signalRef(mouse, INPUT_SIGNAL_MOUSE_BUTTON_MIDDLE);
                if (std::get<bool>(event.data)) {
                    signalRef.SetValue(lock, 1.0);
                } else {
                    signalRef.ClearValue(lock);
                }
            } else if (event.name == INPUT_EVENT_MOUSE_RIGHT_CLICK) {
                ecs::SignalRef signalRef(mouse, INPUT_SIGNAL_MOUSE_BUTTON_RIGHT);
                if (std::get<bool>(event.data)) {
                    signalRef.SetValue(lock, 1.0);
                } else {
                    signalRef.ClearValue(lock);
                }
            }
        });
    }

    void GameLogic::Frame() {
        ZoneScoped;
        {
            auto lock = ecs::StartTransaction<ecs::WriteAll>();
            UpdateInputEvents(lock, windowInputQueue);
            ecs::GetScriptManager().RunOnTick(lock, interval);
        }
    }
} // namespace sp
