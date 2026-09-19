/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "EventScriptManager.hh"

#include "common/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/components/Events.hh"
#include "strayphotons/LockFreeEventQueue.hh"
#include "strayphotons/input/BindingNames.hh"
#include "strayphotons/input/KeyCodes.hh"

namespace sp {
    EventScriptManager::EventScriptManager(LockFreeEventQueue<ecs::Event> &windowInputQueue)
        : windowInputQueue(windowInputQueue), workQueue("EventWorkQueue", 2, std::chrono::milliseconds(1)) {
        (void)this->windowInputQueue;
    }

    void EventScriptManager::UpdateInputEvents(const ecs::Lock<ecs::SendEventsLock, ecs::Write<ecs::Signals>> &lock,
        LockFreeEventQueue<ecs::Event> &inputQueue) {
        ZoneScoped;
        static const ecs::EntityRef keyboardEntity = ecs::Name("input", "keyboard");
        static const ecs::EntityRef mouseEntity = ecs::Name("input", "mouse");
        static const ecs::EntityRef joystickEntity = ecs::Name("input", "joystick0");

        auto keyboard = keyboardEntity.Get(lock);
        auto mouse = mouseEntity.Get(lock);
        auto joystick = joystickEntity.Get(lock);

        inputQueue.PollEvents([&](const ecs::Event &event) {
            if (event.source == keyboard) {
                ecs::EventBindings::SendEvent(lock, keyboardEntity, event);
            } else if (event.source == mouse) {
                ecs::EventBindings::SendEvent(lock, mouseEntity, event);
            } else if (event.source == joystick) {
                ecs::EventBindings::SendEvent(lock, joystickEntity, event);
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

    // void EventScriptManager::Frame() {
    //     ZoneScoped;
    //     auto lock = ecs::StartTransaction<ecs::Write<ecs::Signals>, ecs::ReadAll>();
    //     UpdateInputEvents(lock, windowInputQueue);

    //     auto &signals = lock.Get<const ecs::Signals>().signals;
    //     for (size_t index = 0; index < signals.size(); index++) {
    //         auto &signal = signals[index];
    //         if (signal.ref && signal.lastValueDirty) signal.ref.UpdateDirtySubscribers(lock);
    //     }
    // }
} // namespace sp
