#include "RegisteredThread.hh"

#include "core/Common.hh"
#include "core/Tracing.hh"

#include <array>
#include <numeric>
#include <thread>

namespace sp {
    std::vector<RegisteredThread *> &GetRegisteredThreads() {
        static std::thread::id firstThreadId;
        if (firstThreadId == std::thread::id()) {
            firstThreadId = std::this_thread::get_id();
        } else {
            Assert(firstThreadId == std::this_thread::get_id(),
                "GetRegisteredThreads() must only be called from a single thread");
        }

        static std::vector<RegisteredThread *> registeredThreads;
        return registeredThreads;
    }

    RegisteredThread::RegisteredThread(std::string threadName, chrono_clock::duration interval, bool traceFrames)
        : threadName(threadName), interval(interval), traceFrames(traceFrames), exiting(false) {
        GetRegisteredThreads().emplace_back(this);
    }

    RegisteredThread::RegisteredThread(std::string threadName, double framesPerSecond, bool traceFrames)
        : threadName(threadName), interval(std::chrono::nanoseconds((int64_t)(1e9 / framesPerSecond))),
          traceFrames(traceFrames), exiting(false) {
        GetRegisteredThreads().emplace_back(this);
    }

    RegisteredThread::~RegisteredThread() {
        StopThread();
        auto &threads = GetRegisteredThreads();
        threads.erase(std::find(threads.begin(), threads.end(), this));
    }

    void RegisteredThread::StartThread(bool stepMode) {
        Assert(!thread.joinable(), "RegisteredThread::StartThread() called while thread already running");
        thread = std::thread([this, stepMode] {
            tracy::SetThreadName(threadName.c_str());
            Init();

            auto frameEnd = chrono_clock::now();
#ifdef CATCH_GLOBAL_EXCEPTIONS
            try {
#endif
                while (!this->exiting) {
                    this->FramePreload();
                    if (stepMode) {
                        while (stepCount < maxStepCount) {
                            if (traceFrames) FrameMarkStart(threadName.c_str());
                            this->Frame();
                            if (traceFrames) FrameMarkEnd(threadName.c_str());
                            stepCount++;
                        }
                        stepCount.notify_all();
                    } else {
                        if (traceFrames) FrameMarkStart(threadName.c_str());
                        this->Frame();
                        if (traceFrames) FrameMarkEnd(threadName.c_str());
                    }

                    auto realFrameEnd = chrono_clock::now();

                    if (this->interval.count() > 0) {
                        frameEnd += this->interval;

                        if (realFrameEnd >= frameEnd) {
                            // Falling behind, reset target frame end time.
                            frameEnd = realFrameEnd;
                        }

                        std::this_thread::sleep_until(frameEnd);
                    } else {
                        std::this_thread::yield();
                    }
                }
#ifdef CATCH_GLOBAL_EXCEPTIONS
            } catch (const char *err) {
                Abortf("Exception thrown in %s thread: %s", threadName, err);
            } catch (const std::exception &ex) { Abortf("Exception thrown in %s thread: %s", threadName, ex.what()); }
#endif
        });
    }

    void RegisteredThread::Step(unsigned int count) {
        maxStepCount += count;
        auto step = stepCount.load();
        while (step < maxStepCount) {
            stepCount.wait(step);
            step = stepCount.load();
        }
    }

    void RegisteredThread::StopThread(bool waitForExit) {
        exiting = true;
        if (waitForExit && thread.joinable()) thread.join();
    }

    std::thread::id RegisteredThread::GetThreadId() const {
        return thread.get_id();
    }
} // namespace sp
