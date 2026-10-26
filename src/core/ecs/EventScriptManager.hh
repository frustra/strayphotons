/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EventQueue.hh"
#include "strayphotons/DispatchQueue.hh"
#include "strayphotons/LockFreeEventQueue.hh"
#include "strayphotons/LockFreeMutex.hh"

#include <deque>

namespace ecs {
    class ScriptManager;
    class ScriptState;

    struct PendingScriptEvent {
        Entity ent;
        Event event;
        std::weak_ptr<ScriptState> state;
    };

    class EventScriptManager {
    public:
        EventScriptManager(ScriptManager &scriptManager);

        size_t QueueEventScripts(const Lock<Read<Scripts>> &lock, Entity ent, const Event &event);
        void FlushEventScripts(const DynamicLock<> &lock);

        static void UpdateInputEvents(const DynamicLock<SendEventsLock, Write<Signals>> &lock,
            sp::LockFreeEventQueue<Event> &inputQueue);

    private:
        void ExecuteEventScripts(const DynamicLock<SendEventsLock> &lock,
            std::deque<PendingScriptEvent> &events,
            chrono_clock::time_point timeout);

        ScriptManager &scriptManager;

        sp::LockFreeMutex queuedEventMutex;
        std::deque<PendingScriptEvent> queuedScriptEvents;
        sp::DispatchQueue workQueue;
    };

} // namespace ecs
