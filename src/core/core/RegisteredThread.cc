#include "RegisteredThread.hh"

#include "core/Common.hh"
#include "core/Tracing.hh"

#include <array>
#include <numeric>
#include <thread>

namespace sp {
    RegisteredThread::RegisteredThread(std::string threadName, chrono_clock::duration interval, bool traceFrames)
        : threadName(threadName), interval(interval), traceFrames(traceFrames), exiting(false) {}

    RegisteredThread::RegisteredThread(std::string threadName, double framesPerSecond, bool traceFrames)
        : threadName(threadName), interval(std::chrono::nanoseconds((int64_t)(1e9 / framesPerSecond))),
          traceFrames(traceFrames), exiting(false) {}

    RegisteredThread::~RegisteredThread() {
        StopThread();
    }

    void RegisteredThread::StartThread() {
        Assert(!thread.joinable(), "RegisteredThread::StartThread() called while thread already running");
        thread = std::thread([this] {
            tracy::SetThreadName(threadName.c_str());
            if (!ThreadInit()) return;

            std::array<chrono_clock::duration, 10> previousFrames;
            previousFrames.fill(this->interval);
            size_t frameIndex = 0;

            auto frameEnd = chrono_clock::now();
            auto previousFrameEnd = frameEnd;
#ifdef CATCH_GLOBAL_EXCEPTIONS
            try {
#endif
                while (!this->exiting) {
                    if (traceFrames) FrameMarkStart(threadName.c_str());
                    this->Frame();
                    if (traceFrames) FrameMarkEnd(threadName.c_str());

                    auto realFrameEnd = chrono_clock::now();

                    // Calculate frame rate using a 10-frame rolling window
                    auto frameDelta = realFrameEnd - previousFrameEnd;
                    previousFrames[frameIndex] = frameDelta;
                    frameIndex = (frameIndex + 1) % previousFrames.size();
                    previousFrameEnd = realFrameEnd;
                    auto totalFrameTime = std::accumulate(previousFrames.begin(),
                        previousFrames.end(),
                        chrono_clock::duration::zero());
                    this->averageFrameTimeNs = std::chrono::nanoseconds(totalFrameTime).count() / previousFrames.size();

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

    void RegisteredThread::StopThread(bool waitForExit) {
        exiting = true;
        if (waitForExit && thread.joinable()) thread.join();
    }

    double RegisteredThread::GetFrameRate() const {
        return 1e9 / (double)averageFrameTimeNs.load();
    }

    std::thread::id RegisteredThread::GetThreadId() const {
        return thread.get_id();
    }
} // namespace sp
