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

        std::thread::id GetThreadId() const;

        const std::string threadName;
        const chrono_clock::duration interval;
        const bool traceFrames = false;

    protected:
        void StartThread(bool stepMode = false);
        void Step(unsigned int count = 1);
        void StopThread(bool waitForExit = true);

        // Will be called once per interval except for in step mode
        virtual void Frame() = 0;
        // Will always be called once per interval
        virtual void PreFrame(){};

        // Will be called once in the thread, before the first call to Frame()
        // If this returns false, the thread will be stopped
        virtual bool ThreadInit() {
            return true;
        }

        std::atomic_bool exiting;

    private:
        std::thread thread;

        std::atomic_uint64_t stepCount, maxStepCount;
    };
} // namespace sp
