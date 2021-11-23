#include "PerfTimer.hh"

#include "core/Common.hh"
#include "core/Logging.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
    CVar<bool> CVarProfileCPU("r.ProfileCPU", false, "Display CPU frame timing");
    CVar<bool> CVarProfileGPU("r.ProfileGPU", false, "Display GPU render timing");

    RenderPhase::~RenderPhase() {
        if (timer) { timer->Complete(*this); }
    }

    void RenderPhase::StartTimer(PerfTimer &timer) {
        if (this->timer) return;
        if (!timer.Active()) return;
        this->timer = &timer;
        timer.Register(*this);
    }

    void RenderPhase::StartTimer(CommandContext &cmd, PerfTimer &timer) {
        if (this->timer) return;
        if (!timer.Active()) return;
        this->cmd = &cmd;
        this->timer = &timer;
        timer.Register(*this);
    }

    void PerfTimer::StartFrame() {
        active = CVarProfileCPU.Get() == 1 || CVarProfileGPU.Get() == 1;
    }

    void PerfTimer::EndFrame() {
        Tick();
        frameIndex = (frameIndex + 1) % frames.size();
    }

    void PerfTimer::Register(RenderPhase &phase) {
        auto &query = phase.query;
        auto &frame = Frame();

        if (phase.cmd) {
            if (frame.queryOffset + 2 > frame.queryCount) {
                frame.requiredQueryCount += 2;
                return;
            }
            query.gpuQueries[0] = frame.queryOffset++;
            query.gpuQueries[1] = frame.queryOffset++;

            phase.cmd->Raw().writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe,
                *frame.queryPool,
                query.gpuQueries[0]);
        } else {
            // This phase will aggregate queries from any subphases.
            query.gpuQueries[0] = ~0u;
            query.gpuQueries[1] = ~0u;
        }

        query.resultIndex = frame.results.size();

        TimeResult result;
        result.name = phase.name;
        result.depth = frame.stack.size() + 1;
        frame.results.push_back(result);

        query.registered = true;
        frame.stack.push(&query);

        // Save CPU time as close to start of work as possible.
        query.cpuStart = chrono_clock::now();
    }

    void PerfTimer::Complete(RenderPhase &phase) {
        if (!phase.query.registered) return;

        // Save CPU time as close to end of work as possible.
        phase.query.cpuEnd = chrono_clock::now();

        auto &frame = Frame();

        Assert(frame.stack.top() == &phase.query, "RenderPhase query mismatch");
        frame.stack.pop();

        if (phase.cmd) {
            phase.cmd->Raw().writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe,
                *frame.queryPool,
                phase.query.gpuQueries[1]);
        }

        frame.pending.push_back(phase.query);
    }

    struct GPUQueryResult {
        uint64 timestamp;
    };

    void PerfTimer::Tick() {
        if (!device) return;

        // flush results from two frames ago
        uint32 flushFrameIndex = (frameIndex + frames.size() - 2) % frames.size();
        auto &frame = frames[flushFrameIndex];

        FlushResults(frame);

        if (frame.requiredQueryCount > frame.queryCount) {
            vk::QueryPoolCreateInfo createInfo;
            createInfo.queryCount = frame.requiredQueryCount;
            createInfo.queryType = vk::QueryType::eTimestamp;
            frame.queryPool = (*device)->createQueryPoolUnique(createInfo);

            frame.queryCount = createInfo.queryCount;
            Debugf("updated timestamp query pool size: %d", frame.queryCount);
        }

        if (frame.queryPool && frame.queryCount > 0) {
            auto cmd = device->GetCommandContext();
            cmd->Raw().resetQueryPool(*frame.queryPool, 0, frame.queryCount);
            device->Submit(cmd);
            frame.queryOffset = 0;
        }
    }

    void PerfTimer::FlushResults(FrameContext &frame) {
        if (!frame.queryPool || frame.queryCount == 0) return;

        auto queryCount = frame.queryOffset;
        frame.gpuTimestamps.resize(queryCount);

        auto gpuQueryResult = (*device)->getQueryPoolResults(*frame.queryPool,
            0,
            queryCount,
            sizeof(uint64) * queryCount,
            frame.gpuTimestamps.data(),
            sizeof(uint64),
            vk::QueryResultFlagBits::e64);

        Assert(gpuQueryResult != vk::Result::eNotReady, "perf timer underflow, results not ready");

        while (!frame.pending.empty()) {
            auto query = frame.pending.front();
            uint64 gpuStart, gpuEnd;

            if (query.gpuQueries[0] == ~0u) {
                gpuStart = ~0llu;
                gpuEnd = 0;

                auto depth = frame.results[query.resultIndex].depth;
                for (size_t i = query.resultIndex + 1; i < frame.results.size(); i++) {
                    if (frame.results[i].depth == depth) break;
                    gpuStart = std::min(gpuStart, frame.results[i].gpuStart);
                    gpuEnd = std::max(gpuEnd, frame.results[i].gpuEnd);
                }
            } else {
                Assert(query.gpuQueries[1] == query.gpuQueries[0] + 1,
                    "expected timer to use subsequent query numbers");

                gpuStart = frame.gpuTimestamps[query.gpuQueries[0]];
                gpuEnd = frame.gpuTimestamps[query.gpuQueries[1]];
                if (gpuStart > gpuEnd) {
                    frame.pending.pop_front();
                    continue;
                }
            }

            auto &result = frame.results[query.resultIndex];

            result.cpuElapsed = query.cpuEnd - query.cpuStart;

            result.gpuStart = gpuStart;
            result.gpuEnd = gpuEnd;
            result.gpuElapsed = gpuEnd - gpuStart;
            if (result.gpuElapsed < 0) result.gpuElapsed = 0;

            if (query.resultIndex < lastCompleteFrame.size()) {
                // Smooth out the graph by applying a high-watermark filter.
                auto lastCpuElapsed = lastCompleteFrame[query.resultIndex].cpuElapsed;
                uint64 lastGpuElapsed = lastCompleteFrame[query.resultIndex].gpuElapsed;
                if (result.cpuElapsed < lastCpuElapsed) {
                    result.cpuElapsed = chrono_clock::duration(
                        std::max(result.cpuElapsed.count(), lastCpuElapsed.count() * 99 / 100));
                }
                if (result.gpuElapsed < lastGpuElapsed) {
                    result.gpuElapsed = std::max(result.gpuElapsed, lastGpuElapsed * 99 / 100);
                }
            }

            frame.pending.pop_front();
        }

        lastCompleteFrame = frame.results;
        frame.results.clear();
    }

    bool PerfTimer::Active() {
        return active;
    }
} // namespace sp::vulkan