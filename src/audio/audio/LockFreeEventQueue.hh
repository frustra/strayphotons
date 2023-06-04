#pragma once

#include "core/InlineVector.hh"
#include "core/LockFreeMutex.hh"

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

        void PushEvent(Event &&event) {
            std::lock_guard lock(eventMutex);
            eventBuffer.emplace_back(std::move(event));
        }

    private:
        LockFreeMutex eventMutex;
        InlineVector<Event, MaxQueueSize> eventBuffer;
    };
} // namespace sp
