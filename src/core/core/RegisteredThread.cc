#include "RegisteredThread.hh"

#include "core/Common.hh"

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

    RegisteredThread::RegisteredThread(std::string threadName, chrono_clock::duration interval)
        : threadName(threadName), interval(interval), exiting(false) {
        GetRegisteredThreads().emplace_back(this);
    }

    RegisteredThread::RegisteredThread(std::string threadName, double framesPerSecond)
        : threadName(threadName), interval(std::chrono::nanoseconds((int64_t)(1e9 / framesPerSecond))), exiting(false) {
        GetRegisteredThreads().emplace_back(this);
    }

    RegisteredThread::~RegisteredThread() {
        StopThread();
        auto &threads = GetRegisteredThreads();
        threads.erase(std::find(threads.begin(), threads.end(), this));
    }

    void RegisteredThread::StartThread() {
        Assert(!thread.joinable(), "RegisteredThread::StartThread() called while thread already running");
        thread = std::thread([this] {
            std::array<chrono_clock::duration, 10> previousFrames;
            previousFrames.fill(this->interval);
            size_t frameIndex = 0;

            auto frameEnd = chrono_clock::now();
            auto previousFrameStart = frameEnd - this->interval;
            while (!this->exiting) {
                auto frameStart = chrono_clock::now();

                // Calculate frame rate using a 10-frame rolling window
                previousFrames[frameIndex] = frameStart - previousFrameStart;
                frameIndex = (frameIndex + 1) % previousFrames.size();
                previousFrameStart = frameStart;
                auto totalFrameTime = std::accumulate(previousFrames.begin(),
                                                      previousFrames.end(),
                                                      chrono_clock::duration::zero());
                this->averageFrameTimeNs = std::chrono::nanoseconds(totalFrameTime).count() / previousFrames.size();

                this->Frame();

                if (this->interval.count() > 0) {
                    frameEnd += this->interval;
                    if (frameStart >= frameEnd) {
                        // Falling behind, reset target frame end time.
                        frameEnd = frameStart;
                    }

                    std::this_thread::sleep_until(frameEnd);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    void RegisteredThread::StopThread() {
        exiting = true;
        if (thread.joinable()) thread.join();
    }

    double RegisteredThread::GetFrameRate() const {
        return 1e9 / (double)averageFrameTimeNs.load();
    }
} // namespace sp