#pragma once

#include "core/Common.hh"

#include <atomic>
#include <string>
#include <thread>

namespace sp {
    class RegisteredThread : public NonCopyable {
    public:
        RegisteredThread(std::string threadName, chrono_clock::duration interval, bool traceFrames = false);
        RegisteredThread(std::string threadName, double framesPerSecond, bool traceFrames = false);
        virtual ~RegisteredThread();

        double GetFrameRate() const;
        std::thread::id GetThreadId() const;

        const std::string threadName;
        const chrono_clock::duration interval;
        const bool traceFrames = false;

    protected:
        void StartThread();
        void StopThread(bool waitForExit = true);
        virtual void Frame() = 0;

    private:
        std::thread thread;
        std::atomic_bool exiting;

        std::atomic_int64_t averageFrameTimeNs;
    };

    std::vector<RegisteredThread *> &GetRegisteredThreads();
} // namespace sp
