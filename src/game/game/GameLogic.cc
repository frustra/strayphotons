/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GameLogic.hh"

#include "common/LockFreeEventQueue.hh"
#include "common/Tracing.hh"
#include "console/Console.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptManager.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

namespace sp {
    GameLogic::GameLogic(LockFreeEventQueue<ecs::Event> &windowInputQueue)
        : RegisteredThread("GameLogic", 120.0, true), windowInputQueue(windowInputQueue) {
        funcs.Register<unsigned int>("steplogic",
            "Advance the game logic by N frames, default is 1",
            [this](unsigned int arg) {
                this->Step(std::max(1u, arg));
            });
        funcs.Register("pauselogic", "Pause the game logic thread (See also: resumelogic)", [this] {
            this->Pause(true);
        });
        funcs.Register("resumelogic", "Pause the game logic thread (See also: pauselogic)", [this] {
            this->Pause(false);
        });
    }

    void GameLogic::StartThread(bool startPaused) {
        RegisteredThread::StartThread(startPaused);
    }

    void GameLogic::UpdateInputEvents(const ecs::Lock<ecs::SendEventsLock, ecs::Write<ecs::Signals>> &lock,
        LockFreeEventQueue<ecs::Event> &inputQueue) {
        static const ecs::EntityRef keyboardEntity = ecs::Name("input", "keyboard");
        static const ecs::EntityRef mouseEntity = ecs::Name("input", "mouse");

        auto keyboard = keyboardEntity.Get(lock);
        auto mouse = mouseEntity.Get(lock);

        inputQueue.PollEvents([&](const ecs::Event &event) {
            if (event.source == keyboard) {
                ecs::EventBindings::SendEvent(lock, keyboardEntity, event);
            } else if (event.source == mouse) {
                ecs::EventBindings::SendEvent(lock, mouseEntity, event);
            }

            if (event.name == INPUT_EVENT_KEYBOARD_KEY_DOWN) {
                auto &keyCode = (KeyCode &)ecs::EventData::Get<int>(event.data);
                auto keyName = KeycodeNameLookup.find(keyCode);
                if (keyName != KeycodeNameLookup.end()) {
                    std::string eventName = INPUT_EVENT_KEYBOARD_KEY_BASE + keyName->second;
                    ecs::EventBindings::SendEvent(lock, keyboardEntity, ecs::Event{eventName, keyboard, true});

                    ecs::SignalRef signalRef(keyboard, INPUT_SIGNAL_KEYBOARD_KEY_BASE + keyName->second);
                    signalRef.SetValue(lock, 1.0);
                }
            } else if (event.name == INPUT_EVENT_KEYBOARD_KEY_UP) {
                auto &keyCode = (KeyCode &)ecs::EventData::Get<int>(event.data);
                auto keyName = KeycodeNameLookup.find(keyCode);
                if (keyName != KeycodeNameLookup.end()) {
                    std::string eventName = INPUT_EVENT_KEYBOARD_KEY_BASE + keyName->second;
                    ecs::EventBindings::SendEvent(lock, keyboardEntity, ecs::Event{eventName, keyboard, false});

                    ecs::SignalRef signalRef(keyboard, INPUT_SIGNAL_KEYBOARD_KEY_BASE + keyName->second);
                    signalRef.ClearValue(lock);
                }
            } else if (event.name == INPUT_EVENT_MOUSE_POSITION) {
                auto &mousePos = ecs::EventData::Get<glm::vec2>(event.data);
                ecs::SignalRef refX(mouse, INPUT_SIGNAL_MOUSE_CURSOR_X);
                ecs::SignalRef refY(mouse, INPUT_SIGNAL_MOUSE_CURSOR_Y);
                refX.SetValue(lock, mousePos.x);
                refY.SetValue(lock, mousePos.y);
            } else if (event.name == INPUT_EVENT_MOUSE_LEFT_CLICK) {
                ecs::SignalRef signalRef(mouse, INPUT_SIGNAL_MOUSE_BUTTON_LEFT);
                if (ecs::EventData::Get<bool>(event.data)) {
                    signalRef.SetValue(lock, 1.0);
                } else {
                    signalRef.ClearValue(lock);
                }
            } else if (event.name == INPUT_EVENT_MOUSE_MIDDLE_CLICK) {
                ecs::SignalRef signalRef(mouse, INPUT_SIGNAL_MOUSE_BUTTON_MIDDLE);
                if (ecs::EventData::Get<bool>(event.data)) {
                    signalRef.SetValue(lock, 1.0);
                } else {
                    signalRef.ClearValue(lock);
                }
            } else if (event.name == INPUT_EVENT_MOUSE_RIGHT_CLICK) {
                ecs::SignalRef signalRef(mouse, INPUT_SIGNAL_MOUSE_BUTTON_RIGHT);
                if (ecs::EventData::Get<bool>(event.data)) {
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
        {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::Signals>, ecs::ReadAll>();
            auto &signals = lock.Get<const ecs::Signals>().signals;
            for (size_t index = 0; index < signals.size(); index++) {
                auto &signal = signals[index];
                if (signal.ref && signal.lastValueDirty) signal.ref.UpdateDirtySubscribers(lock);
            }
        }
    }
} // namespace sp
