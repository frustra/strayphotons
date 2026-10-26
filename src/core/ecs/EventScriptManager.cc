/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "EventScriptManager.hh"

#include "common/Tracing.hh"
#include "ecs/DynamicLibrary.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EventQueue.hh"
#include "ecs/ScriptManager.hh"
#include "ecs/components/Events.hh"
#include "strayphotons/DispatchQueue.hh"
#include "strayphotons/LockFreeEventQueue.hh"
#include "strayphotons/LockFreeMutex.hh"
#include "strayphotons/Logging.hh"
#include "strayphotons/input/BindingNames.hh"
#include "strayphotons/input/KeyCodes.hh"

#include <deque>
#include <mutex>
#include <optional>

namespace ecs {
    EventScriptManager::EventScriptManager(ScriptManager &scriptManager)
        : scriptManager(scriptManager), workQueue("EventWorkQueue", 2, std::chrono::milliseconds(1)) {}

    thread_local std::deque<PendingScriptEvent> pendingScriptEvents;
    thread_local bool flushingQueuedEvents = false;

    size_t EventScriptManager::QueueEventScripts(const Lock<Read<Scripts>> &lock, Entity ent, const Event &event) {
        ZoneScoped;
        if (!flushingQueuedEvents) scriptManager.scriptSet.eventScriptMutex.lock_shared();

        size_t eventCount = 0;
        auto &scripts = ent.Get<const Scripts>(lock);
        for (auto &instance : scripts.scripts) {
            if (!instance) continue;
            auto &state = *instance.state;
            auto &eventFunc = state.definition.onEventFunc;
            if (!eventFunc) continue;
            if (!sp::contains(state.definition.events, event.name)) continue;
            pendingScriptEvents.emplace_back(ent, event, instance.state);
            eventCount++;
        }
        if (!flushingQueuedEvents) scriptManager.scriptSet.eventScriptMutex.unlock_shared();
        return eventCount;
    }

    void EventScriptManager::FlushEventScripts(const DynamicLock<> &lock) {
        ZoneScoped;
        if (pendingScriptEvents.empty()) return;
        std::lock_guard l1(queuedEventMutex);
        std::lock_guard l2(scriptManager.scriptSet.eventScriptMutex);
        DebugAssertf(!flushingQueuedEvents, "EventScriptManager::FlushEventScripts called recursively");
        flushingQueuedEvents = true;

        auto tryLock = lock.TryLock<SendEventsLock>();
        if (tryLock.has_value()) {
            DynamicLock<SendEventsLock> dynamicLock(*tryLock,
                lock.GetDynamicReadPermissions() & ~Lock<ReadPrevious>::GetReadPermissions(),
                lock.GetDynamicWritePermissions());

            chrono_clock::time_point timeout = chrono_clock::now() + workQueue.flushSleepInterval;
            // Try executing already queued events first
            ExecuteEventScripts(dynamicLock, queuedScriptEvents, timeout);
            // Only execute new pending events if all queued events a finished
            if (queuedScriptEvents.empty()) ExecuteEventScripts(dynamicLock, pendingScriptEvents, timeout);
        }

        PermissionBitset requiredReadPermissions, requiredWritePermissions;
        for (PendingScriptEvent &queued : queuedScriptEvents) {
            auto statePtr = queued.state.lock();
            if (!statePtr) continue;
            auto &state = *statePtr;
            auto &eventFunc = state.definition.onEventFunc;
            DebugAssertf(eventFunc.readPermissions.test(0) == 0,
                "ReadPrevious permissions not allowed on event script: %s",
                state.definition.name);
            requiredReadPermissions |= eventFunc.readPermissions;
            requiredWritePermissions |= eventFunc.writePermissions;
        }
        for (const PendingScriptEvent &pending : pendingScriptEvents) {
            auto statePtr = pending.state.lock();
            if (!statePtr) continue;
            auto &state = *statePtr;
            auto &eventFunc = state.definition.onEventFunc;
            DebugAssertf(eventFunc.readPermissions.test(0) == 0,
                "ReadPrevious permissions not allowed on event script: %s",
                state.definition.name);
            requiredReadPermissions |= eventFunc.readPermissions;
            requiredWritePermissions |= eventFunc.writePermissions;
            queuedScriptEvents.emplace_back(pending);
        }
        pendingScriptEvents.clear();

        if (!queuedScriptEvents.empty()) {
            // Queue remaining unprocessed events in a new transaction
            workQueue.Dispatch<void>(NewDispatchSource, [this, requiredReadPermissions, requiredWritePermissions]() {
                ZoneScopedN("FlushEventScriptsAsync");
                DynamicLock<> dynamicLock(World(),
                    requiredReadPermissions | SendEventsLock::GetReadPermissions(),
                    requiredWritePermissions);
                auto tryLock = dynamicLock.TryLock<SendEventsLock>();
                Assertf(tryLock.has_value(), "FlushEventScripts() returned dynamic lock with missing permissions");

                std::lock_guard l1(queuedEventMutex);
                std::lock_guard l2(scriptManager.scriptSet.eventScriptMutex);
                chrono_clock::time_point timeout = chrono_clock::now() + workQueue.flushSleepInterval;
                DebugAssertf(!flushingQueuedEvents, "EventScriptManager::FlushEventScriptsAsync called recursively");
                flushingQueuedEvents = true;
                ExecuteEventScripts(*tryLock, queuedScriptEvents, timeout);
                flushingQueuedEvents = false;
            });
        }

        flushingQueuedEvents = false;
    }

    void EventScriptManager::ExecuteEventScripts(const DynamicLock<SendEventsLock> &lock,
        std::deque<PendingScriptEvent> &events,
        chrono_clock::time_point timeout) {
        Assertf(lock.GetDynamicReadPermissions().test(0) == 0,
            "EventScriptManager::ExecuteEventScripts() unexpected ReadPrevious permission");
        while (!events.empty() && chrono_clock::now() < timeout) {
            auto &queued = events.front();
            auto statePtr = queued.state.lock();
            if (!statePtr) continue;
            auto &state = *statePtr;
            auto &eventFunc = state.definition.onEventFunc;
            if (!eventFunc) continue;
            if (!sp::contains(state.definition.events, queued.event.name)) continue;
            if ((eventFunc.readPermissions & lock.GetDynamicReadPermissions()) == eventFunc.readPermissions &&
                (eventFunc.writePermissions & lock.GetDynamicWritePermissions()) == eventFunc.writePermissions) {
                eventFunc.callback(state, lock, queued.ent, queued.event);
                events.pop_front();
            } else {
                // Not all lock permissions available, remaining events are deferred
                return;
            }
        }
    }

    void EventScriptManager::UpdateInputEvents(const DynamicLock<SendEventsLock, Write<Signals>> &lock,
        sp::LockFreeEventQueue<Event> &inputQueue) {
        ZoneScoped;
        static const EntityRef keyboardEntity = Name("input", "keyboard");
        static const EntityRef mouseEntity = Name("input", "mouse");
        static const EntityRef joystickEntity = Name("input", "joystick0");

        auto keyboard = keyboardEntity.Get(lock);
        auto mouse = mouseEntity.Get(lock);
        auto joystick = joystickEntity.Get(lock);

        inputQueue.PollEvents([&](const Event &event) {
            if (event.source == keyboard) {
                EventBindings::SendEvent(lock, keyboardEntity, event);
            } else if (event.source == mouse) {
                EventBindings::SendEvent(lock, mouseEntity, event);
            } else if (event.source == joystick) {
                EventBindings::SendEvent(lock, joystickEntity, event);
            }

            if (event.name == sp::INPUT_EVENT_KEYBOARD_KEY_DOWN) {
                auto &keyCode = (sp::KeyCode &)EventData::Get<int>(event.data);
                auto keyName = sp::KeycodeNameLookup.find(keyCode);
                if (keyName != sp::KeycodeNameLookup.end()) {
                    std::string eventName = sp::INPUT_EVENT_KEYBOARD_KEY_BASE + keyName->second;
                    EventBindings::SendEvent(lock, keyboardEntity, Event{eventName, keyboard, true});

                    SignalRef signalRef(keyboard, sp::INPUT_SIGNAL_KEYBOARD_KEY_BASE + keyName->second);
                    signalRef.SetValue(lock, 1.0);
                }
            } else if (event.name == sp::INPUT_EVENT_KEYBOARD_KEY_UP) {
                auto &keyCode = (sp::KeyCode &)EventData::Get<int>(event.data);
                auto keyName = sp::KeycodeNameLookup.find(keyCode);
                if (keyName != sp::KeycodeNameLookup.end()) {
                    std::string eventName = sp::INPUT_EVENT_KEYBOARD_KEY_BASE + keyName->second;
                    EventBindings::SendEvent(lock, keyboardEntity, Event{eventName, keyboard, false});

                    SignalRef signalRef(keyboard, sp::INPUT_SIGNAL_KEYBOARD_KEY_BASE + keyName->second);
                    signalRef.ClearValue(lock);
                }
            } else if (event.name == sp::INPUT_EVENT_MOUSE_POSITION) {
                auto &mousePos = EventData::Get<glm::vec2>(event.data);
                SignalRef refX(mouse, sp::INPUT_SIGNAL_MOUSE_CURSOR_X);
                SignalRef refY(mouse, sp::INPUT_SIGNAL_MOUSE_CURSOR_Y);
                refX.SetValue(lock, mousePos.x);
                refY.SetValue(lock, mousePos.y);
            } else if (event.name == sp::INPUT_EVENT_MOUSE_LEFT_CLICK) {
                SignalRef signalRef(mouse, sp::INPUT_SIGNAL_MOUSE_BUTTON_LEFT);
                if (EventData::Get<bool>(event.data)) {
                    signalRef.SetValue(lock, 1.0);
                } else {
                    signalRef.ClearValue(lock);
                }
            } else if (event.name == sp::INPUT_EVENT_MOUSE_MIDDLE_CLICK) {
                SignalRef signalRef(mouse, sp::INPUT_SIGNAL_MOUSE_BUTTON_MIDDLE);
                if (EventData::Get<bool>(event.data)) {
                    signalRef.SetValue(lock, 1.0);
                } else {
                    signalRef.ClearValue(lock);
                }
            } else if (event.name == sp::INPUT_EVENT_MOUSE_RIGHT_CLICK) {
                SignalRef signalRef(mouse, sp::INPUT_SIGNAL_MOUSE_BUTTON_RIGHT);
                if (EventData::Get<bool>(event.data)) {
                    signalRef.SetValue(lock, 1.0);
                } else {
                    signalRef.ClearValue(lock);
                }
            }
        });
    }
} // namespace ecs
