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

        void Step(unsigned int count = 1);
        std::thread::id GetThreadId() const;

        const std::string threadName;
        chrono_clock::duration interval;
        std::atomic_uint64_t stepCount, maxStepCount;
        const bool traceFrames = false;

    protected:
        void StartThread(bool stepMode = false);
        void StopThread(bool waitForExit = true);

        // Will be called once per interval except for in step mode
        virtual void Frame() = 0;
        // Will always be called once per interval
        virtual void PreFrame(){};
        // Will always be called once per interval
        virtual void PostFrame(){};

        // Will be called once in the thread, before the first call to Frame()
        // If this returns false, the thread will be stopped
        virtual bool ThreadInit() {
            return true;
        }

        std::atomic_bool exiting;
        std::atomic_bool exited;

    private:
        std::thread thread;
    };
} // namespace sp
