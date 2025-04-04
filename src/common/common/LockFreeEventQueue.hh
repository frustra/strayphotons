/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/InlineVector.hh"
#include "common/LockFreeMutex.hh"

#include <vector>

namespace sp {
    template<typename Event, size_t MaxQueueSize = 1000>
    class LockFreeEventQueue {
    public:
        LockFreeEventQueue() {}

        template<typename Fn>
        bool TryPollEvents(Fn &&eventCallback) {
            if (eventMutex.try_lock()) {
                for (auto &event : eventBuffer) {
                    eventCallback(event);
                }
                eventBuffer.clear();
                eventMutex.unlock();
                return true;
            }
            return false;
        }

        template<typename Fn>
        void PollEvents(Fn &&eventCallback) {
            std::lock_guard lock(eventMutex);
            for (auto &event : eventBuffer) {
                eventCallback(event);
            }
            eventBuffer.clear();
        }

        void PushEvent(Event &&event) {
            std::lock_guard lock(eventMutex);
            if (eventBuffer.size() < eventBuffer.capacity()) {
                eventBuffer.emplace_back(std::move(event));
            } else {
                Errorf("LockFreeEventQueue full! Dropping event %s", typeid(Event).name());
            }
        }

    private:
        LockFreeMutex eventMutex;
        InlineVector<Event, MaxQueueSize> eventBuffer;
    };
} // namespace sp
