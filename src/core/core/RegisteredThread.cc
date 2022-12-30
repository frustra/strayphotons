#include "RegisteredThread.hh"

#include "core/Common.hh"
#include "core/Defer.hh"
#include "core/Tracing.hh"

#include <array>
#include <iostream>
#include <numeric>
#include <thread>

namespace sp {
    RegisteredThread::RegisteredThread(std::string threadName, chrono_clock::duration interval, bool traceFrames)
        : threadName(threadName), interval(interval), traceFrames(traceFrames), state(ThreadState::Stopped) {}

    RegisteredThread::RegisteredThread(std::string threadName, double framesPerSecond, bool traceFrames)
        : threadName(threadName), interval(0), traceFrames(traceFrames), state(ThreadState::Stopped) {
        if (framesPerSecond > 0.0) {
            interval = std::chrono::nanoseconds((int64_t)(1e9 / framesPerSecond));
        }
    }

    RegisteredThread::~RegisteredThread() {
        StopThread();
        if (thread.joinable()) thread.join();
    }

    void RegisteredThread::StartThread(bool stepMode) {
        ThreadState current = state;
        if (current != ThreadState::Stopped || !state.compare_exchange_strong(current, ThreadState::Started)) {
            Errorf("RegisteredThread %s already started: %s", threadName, current);
            return;
        }
        state.notify_all();

        thread = std::thread([this, stepMode] {
            tracy::SetThreadName(threadName.c_str());
            Defer exit([this] {
                ThreadState current = state;
                if (current == ThreadState::Stopped || !state.compare_exchange_strong(current, ThreadState::Stopped)) {
                    Errorf("RegisteredThread %s state already Stopped", threadName);
                }
                state.notify_all();
            });

            if (!ThreadInit()) return;

            auto frameEnd = chrono_clock::now();
#ifdef CATCH_GLOBAL_EXCEPTIONS
            try {
#endif
                while (state == ThreadState::Started) {
                    this->PreFrame();
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
                    this->PostFrame();

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
            } catch (const std::exception &ex) {
                Abortf("Exception thrown in %s thread: %s", threadName, ex.what());
            }
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
        ThreadState current = state;
        if (current == ThreadState::Stopped || !state.compare_exchange_strong(current, ThreadState::Stopping)) {
            // Thread already in a stopped state
            return;
        }
        state.notify_all();

        if (waitForExit) {
            while (current != ThreadState::Stopped) {
                state.wait(current);
                current = state;
            }
        }
    }

    std::thread::id RegisteredThread::GetThreadId() const {
        return thread.get_id();
    }
} // namespace sp
