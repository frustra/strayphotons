#pragma once

#include "core/Common.hh"

#include <atomic>
#include <string>
#include <thread>

namespace sp {
    class RegisteredThread : public NonCopyable {
    public:
        RegisteredThread(std::string threadName, chrono_clock::duration interval);
        RegisteredThread(std::string threadName, double framesPerSecond);
        virtual ~RegisteredThread();

        double GetFrameRate() const;
        std::thread::id GetThreadId() const;

        const std::string threadName;
        const chrono_clock::duration interval;

    protected:
        void StartThread();
        void StopThread();
        virtual void Frame() = 0;

    private:
        std::thread thread;
        std::atomic_bool exiting;

        std::atomic_int64_t averageFrameTimeNs;
    };

    std::vector<RegisteredThread *> &GetRegisteredThreads();
} // namespace sp
