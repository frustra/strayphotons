#pragma once

#include "console/CVar.hh"
#include "core/Common.hh"
#include "graphics/vulkan/core/VkCommon.hh"

#include <queue>
#include <stack>

namespace sp::vulkan {
    class PerfTimer;
    extern CVar<bool> CVarProfileRender;

    struct TimeResult {
        string name;
        size_t depth = 0;
        chrono_clock::duration cpuElapsed;
        uint64 gpuElapsed = 0;

        // used to propagate GPU elapsed time down the phase stack
        uint64 gpuStart = ~0llu, gpuEnd = 0;
    };

    struct TimeQuery {
        chrono_clock::time_point cpuStart, cpuEnd;
        uint32_t gpuQueries[2];
        size_t resultIndex;
        bool registered = false;
    };

    class RenderPhase {
    public:
        const string_view name;
        PerfTimer *timer = nullptr;
        CommandContext *cmd = nullptr;
        TimeQuery query;

        RenderPhase(string_view phaseName) : name(phaseName) {}

        void StartTimer(PerfTimer &timer);
        void StartTimer(CommandContext &cmd);

        ~RenderPhase();
    };

    class PerfTimer {
    public:
        PerfTimer(DeviceContext &device) : device(device) {}

        void StartFrame();
        void EndFrame();

        void Register(RenderPhase &phase);
        void Complete(RenderPhase &phase);

        void Tick();
        bool Active();

        vector<TimeResult> lastCompleteFrame;

    private:
        DeviceContext &device;
        bool active = false;

        struct FrameContext {
            vk::UniqueQueryPool queryPool;
            uint32 queryOffset = 0;
            uint32 queryCount = 0;
            uint32 requiredQueryCount = 0;

            std::stack<TimeQuery *> stack;
            std::deque<TimeQuery> pending;

            vector<uint64> gpuTimestamps;
            vector<TimeResult> results;
        };

        std::array<FrameContext, 4> frames;
        uint32 frameIndex = 0;

        FrameContext &Frame() {
            return frames[frameIndex];
        }

        bool FlushResults(FrameContext &frame);
    };
} // namespace sp::vulkan
