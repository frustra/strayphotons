/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "RegisteredThread.hh"

#include "common/Common.hh"
#include "common/Defer.hh"
#include "common/Hashing.hh"
#include "common/LockFreeMutex.hh"
#include "common/Tracing.hh"

#include <array>
#include <iostream>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <thread>

namespace sp {

    LockFreeMutex &getRegistrationMutex() {
        static LockFreeMutex registrationMutex;
        return registrationMutex;
    }
    robin_hood::unordered_map<std::string, const RegisteredThread *, StringHash, StringEqual> &getRegisteredThreads() {
        static robin_hood::unordered_map<std::string, const RegisteredThread *, StringHash, StringEqual>
            registeredThreads;
        return registeredThreads;
    }

    void registerThread(const RegisteredThread &thread) {
        std::lock_guard l(getRegistrationMutex());
        getRegisteredThreads().emplace(thread.threadName, &thread);
    }

    void unregisterThread(std::string_view threadName) {
        std::lock_guard l(getRegistrationMutex());
        getRegisteredThreads().erase(std::string(threadName));
    }

    RegisteredThread::RegisteredThread(std::string threadName, chrono_clock::duration interval, bool traceFrames)
        : threadName(threadName), interval(interval), traceFrames(traceFrames), state(ThreadState::Stopped) {
        registerThread(*this);
    }

    RegisteredThread::RegisteredThread(std::string threadName, double framesPerSecond, bool traceFrames)
        : threadName(threadName), interval(0), traceFrames(traceFrames), state(ThreadState::Stopped) {
        if (framesPerSecond > 0.0) {
            interval = std::chrono::nanoseconds((int64_t)(1e9 / framesPerSecond));
        }
        registerThread(*this);
    }

    RegisteredThread::~RegisteredThread() {
        StopThread();
        unregisterThread(threadName);
        if (thread.joinable()) thread.join();
    }

    void RegisteredThread::StartThread(bool startPaused) {
        ThreadState current = state;
        if (current != ThreadState::Stopped || !state.compare_exchange_strong(current, ThreadState::Started)) {
            Errorf("RegisteredThread %s already started: %s", threadName, current);
            return;
        }
        state.notify_all();

        if (startPaused) this->Pause();

        thread = std::thread([this] {
            tracy::SetThreadName(threadName.c_str());
            Tracef("RegisteredThread Started %s", threadName);
            Defer exit([this] {
                Tracef("Thread stopping: %s", threadName);
                ThreadState current = state;
                if (current == ThreadState::Stopped || !state.compare_exchange_strong(current, ThreadState::Stopped)) {
                    Errorf("RegisteredThread %s state already Stopped", threadName);
                } else {
                    ThreadShutdown();
                }
                Tracef("Thread stopped: %s", threadName);
                state.notify_all();
            });

            if (!ThreadInit()) return;

            auto targetFrameEnd = chrono_clock::now() + this->interval;
            uint32_t fpsCounter = 0;
            chrono_clock::time_point fpsTimer = chrono_clock::now();
#ifdef CATCH_GLOBAL_EXCEPTIONS
            try {
#endif
                while (state == ThreadState::Started) {
                    if (this->PreFrame()) {
                        if (this->stepMode) {
                            while (stepCount < maxStepCount) {
                                if (traceFrames) FrameMarkStart(threadName.c_str());
                                this->Frame();
                                if (traceFrames) FrameMarkEnd(threadName.c_str());
                                stepCount++;
                            }
                            stepCount.notify_all();
                            this->PostFrame(true);
                        } else {
                            if (traceFrames) FrameMarkStart(threadName.c_str());
                            this->Frame();
                            if (traceFrames) FrameMarkEnd(threadName.c_str());
                            this->PostFrame(false);
                        }
                        // Logf("[%s] Allocations per frame: %llu", threadName, SampleAllocationCount());fpsMeasuredTime
                        fpsCounter++;
                    }

                    auto realFrameEnd = chrono_clock::now();
                    if (realFrameEnd - fpsTimer > std::chrono::seconds(1)) {
                        measuredFps = fpsCounter;
                        fpsCounter = 0;
                        fpsTimer = chrono_clock::now();
                    }

                    if (this->interval.count() > 0) {
                        targetFrameEnd += this->interval;

                        if (realFrameEnd < targetFrameEnd) {
                            std::this_thread::sleep_until(targetFrameEnd);
                        } else {
                            // std::cout << std::string("Thread behind: " + threadName + " by " +
                            //                          std::to_string((realFrameEnd - targetFrameEnd).count() / 1000) +
                            //                          "\n");

                            // Falling behind, reset target frame end time.
                            // Add some extra time to allow other threads to start transactions.
                            targetFrameEnd = realFrameEnd + std::chrono::nanoseconds(100);
                            std::this_thread::yield();
                        }

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

    void RegisteredThread::Pause(bool pause) {
        stepMode = pause;
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

    uint32_t GetMeasuredFps_static(std::string_view threadName) {
        std::shared_lock l(getRegistrationMutex());
        auto &registeredThreads = getRegisteredThreads();
        auto it = registeredThreads.find(threadName);
        if (it != registeredThreads.end()) {
            return it->second->GetMeasuredFps();
        } else {
            return 0;
        }
    }
} // namespace sp
