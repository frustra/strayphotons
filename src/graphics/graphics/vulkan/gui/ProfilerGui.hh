/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/components/Gui.hh"
#include "graphics/core/Histogram.hh"
#include "graphics/vulkan/core/PerfTimer.hh"

#include <imgui/imgui.h>
#include <sstream>

namespace sp::vulkan {
    class ProfilerGui final : public ecs::GuiRenderable {
    public:
        enum class Mode {
            CPU,
            GPU,
        };

        ProfilerGui(PerfTimer &timer)
            : ecs::GuiRenderable("profiler",
                  ecs::GuiLayoutAnchor::Floating,
                  {-1, -1},
                  ImGuiWindowFlags_AlwaysAutoResize),
              timer(timer), msWindowSize(1000) {}
        virtual ~ProfilerGui() {}

        static float GetHistogramValue(void *data, int index) {
            auto self = static_cast<ProfilerGui *>(data);
            return (float)self->drawHistogram.buckets[index];
        }

        bool PreDefine(ecs::Entity ent) override {
            if (timer.lastCompleteFrame.empty()) return false;
            if (!CVarProfileRender.Get()) {
                // Auto-resize on first frame shown
                windowFlags |= ImGuiWindowFlags_AlwaysAutoResize;
                return false;
            }
            ZoneScoped;

            CollectSample();
            return true;
        }

        void DefineContents(ecs::Entity ent) override {
            ZoneScoped;

            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImGui::GetStyleColorVec4(ImGuiCol_ChildBg));
            if (ImGui::BeginTable("ResultTable", 7)) {
                ImGui::TableSetupColumn("Time per frame (ms)");
                ImGui::TableSetupColumn("CPU avg", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("CPU p95", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("CPU max", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("GPU avg", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("GPU p95", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("GPU max", ImGuiTableColumnFlags_WidthFixed);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TableHeader("Time per frame (ms)");
                ImGui::Text("%d ms window%s",
                    msWindowSize,
                    SpacePadding(msWindowSize < 1000    ? 2
                                 : msWindowSize < 10000 ? 1
                                                        : 0));
                ImGui::SameLine();
                if (ImGui::SmallButton("-")) {
                    if (msWindowSize <= 1000) {
                        if (msWindowSize > 100) msWindowSize -= 100;
                    } else {
                        msWindowSize -= 1000;
                    }
                }
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                if (ImGui::SmallButton("+")) {
                    if (msWindowSize < 1000) {
                        msWindowSize += 100;
                    } else {
                        msWindowSize += 1000;
                    }
                }
                ImGui::TableNextColumn();
                ImGui::TableHeader("CPU  \navg##CPUavg");
                ImGui::TableNextColumn();
                ImGui::TableHeader("     \np95##CPUp95");
                ImGui::TableNextColumn();
                ImGui::TableHeader("     \nmax##CPUmax");
                ImGui::TableNextColumn();
                ImGui::TableHeader("GPU  \navg##GPUavg");
                ImGui::TableNextColumn();
                ImGui::TableHeader("     \np95##GPUp95");
                ImGui::TableNextColumn();
                ImGui::TableHeader("     \nmax##GPUmax");

                AddResults();
                ImGui::EndTable();
            }
            ImGui::PopStyleColor();

            ImGui::SetNextItemWidth(-1);
            ImGui::PlotHistogram("##histogram",
                &GetHistogramValue,
                this,
                drawHistogram.buckets.size(),
                0,
                nullptr,
                FLT_MAX,
                FLT_MAX,
                ImVec2(0, 100));

            ImGui::Text("%6.3f", drawHistogram.min / 1000000.0);
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x / 2 - ImGui::GetItemRectSize().x / 2);
            ImGui::Text("%6.3f", ((drawHistogram.max - drawHistogram.min) / 2 + drawHistogram.min) / 1000000.0);
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::GetItemRectSize().x);
            ImGui::Text("%6.3f", drawHistogram.max / 1000000.0);

            if (drawHistogramMode == Mode::CPU) {
                if (ImGui::Button("CPU histogram")) drawHistogramMode = Mode::GPU;
            } else {
                if (ImGui::Button("GPU histogram")) drawHistogramMode = Mode::CPU;
            }
            ImGui::SameLine();
            if (histogramLocked && ImGui::Button("Unlock histogram")) histogramLocked = false;

            windowFlags &= ~ImGuiWindowFlags_AlwaysAutoResize;
        }

    private:
        struct Sample {
            uint64 cpuElapsed = 0, gpuElapsed = 0;
        };

        struct Scope {
            std::string name;
            size_t depth = 0;
            size_t sampleOffset = 0, sampleCount = 0;
            bool truncated = false;

            std::array<Sample, 1000> samples;
        };

        struct Stats {
            struct {
                uint64 avg = 0, p95 = 0, max = 0, min = std::numeric_limits<uint64>::max();
            } cpu, gpu;
        };

        static constexpr string_view spacePadding = "               ";

        const char *SpacePadding(size_t spaceCount) {
            size_t paddingOffset = std::max((int)spacePadding.size() - (int)spaceCount, 0);
            return spacePadding.data() + paddingOffset;
        }

        size_t AddResults(size_t offset = 0, size_t depth = 1) {
            while (offset < resultCount) {
                const auto &scope = resultScopes[offset];

                if (scope.depth < depth) return offset;
                if (scope.depth > depth) continue;

                auto stats = GetStats(scope);

                ImGui::TableNextRow();

                float c = 0.1 * ((offset % 2) + 1);

                if (offset == drawHistogramIndex) {
                    UpdateDrawHistogram(scope, stats);
                    if (histogramLocked) {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32({0.1, 0.3, 0.1, 0.6}));
                    } else {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32({0.1, 0.1, 0.3, 0.6}));
                    }
                } else {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32({c, c, c, 0.6}));
                }

                ImGui::TableNextColumn();

                ImGui::Text("%s%s", SpacePadding(depth * 2 - 1), scope.name.data());

                auto windowX = ImGui::GetWindowPos().x;
                auto rowMin = ImGui::GetItemRectMin();
                rowMin.x = ImGui::GetWindowContentRegionMin().x + windowX;
                auto rowMax = ImGui::GetItemRectMax();
                rowMax.x = ImGui::GetWindowContentRegionMax().x + windowX;
                if (drawHistogramIndex != offset && ImGui::IsMouseHoveringRect(rowMin, rowMax, false)) {
                    if (!histogramLocked || ImGui::IsMouseClicked(0)) drawHistogramIndex = offset;
                }

                ImGui::TableNextColumn();
                ImGui::Text("%.2f", stats.cpu.avg / 1000000.0);
                HandleMouse(Mode::CPU);
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", stats.cpu.p95 / 1000000.0);
                HandleMouse(Mode::CPU);
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", stats.cpu.max / 1000000.0);
                HandleMouse(Mode::CPU);

                ImGui::TableNextColumn();
                ImGui::Text("%.2f", stats.gpu.avg / 1000000.0);
                HandleMouse(Mode::GPU);
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", stats.gpu.p95 / 1000000.0);
                HandleMouse(Mode::GPU);
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", stats.gpu.max / 1000000.0);
                HandleMouse(Mode::GPU);

                offset = AddResults(offset + 1, scope.depth + 1);
            }
            return offset;
        }

        void HandleMouse(Mode newMode) {
            if (histogramLocked) return;
            if (ImGui::IsItemHovered()) {
                drawHistogramMode = newMode;
                if (ImGui::IsMouseClicked(0)) histogramLocked = true;
            }
        }

        void CollectSample() {
            auto now = chrono_clock::now();
            bool newWindow = false;
            if ((now - lastWindowStart) > std::chrono::milliseconds(msWindowSize)) {
                newWindow = true;
                lastWindowStart = now;

                uint64 dhWindow = drawHistogram.max - drawHistogram.min;
                drawHistogram.min += dhWindow * 0.05;
                drawHistogram.max -= dhWindow * 0.05;
            }

            resultCount = timer.lastCompleteFrame.size();
            if (resultScopes.size() < resultCount) resultScopes.resize(resultCount);

            for (size_t i = 0; i < resultCount; i++) {
                auto &result = timer.lastCompleteFrame[i];
                auto &scope = resultScopes[i];
                scope.depth = result.depth;

                if (result.name != scope.name) {
                    scope.name = result.name;
                    scope.sampleCount = 0;
                    scope.sampleOffset = 0;
                    scope.truncated = false;
                }

                if (newWindow) {
                    if (!scope.truncated) scope.sampleCount = scope.sampleOffset;
                    scope.sampleOffset = 0;
                    scope.truncated = false;
                } else {
                    scope.sampleOffset++;
                    if (scope.sampleOffset >= scope.samples.size()) {
                        scope.sampleOffset = 0;
                        scope.truncated = true;
                    }
                    if (!scope.truncated) scope.sampleCount = std::max(scope.sampleCount, scope.sampleOffset);
                }

                auto &sample = scope.samples[scope.sampleOffset];
                sample.cpuElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(result.cpuElapsed).count();
                sample.gpuElapsed = result.gpuElapsed;
            }
        }

        Stats GetStats(const Scope &scope) {
            Stats stats;
            if (scope.sampleCount == 0) return stats;

            for (size_t i = 0; i < scope.sampleCount; i++) {
                auto &sample = scope.samples[i];
                stats.cpu.avg += sample.cpuElapsed;
                stats.gpu.avg += sample.gpuElapsed;
                stats.cpu.max = std::max(stats.cpu.max, sample.cpuElapsed);
                stats.gpu.max = std::max(stats.gpu.max, sample.gpuElapsed);
                stats.cpu.min = std::min(stats.cpu.min, sample.cpuElapsed);
                stats.gpu.min = std::min(stats.gpu.min, sample.gpuElapsed);
            }
            stats.cpu.avg /= scope.sampleCount;
            stats.gpu.avg /= scope.sampleCount;

            histogram.Reset(stats.cpu.min, stats.cpu.max);
            for (size_t i = 0; i < scope.sampleCount; i++) {
                histogram.AddSample(scope.samples[i].cpuElapsed);
            }
            stats.cpu.p95 = histogram.GetPercentile(95);

            histogram.Reset(stats.gpu.min, stats.gpu.max);
            for (size_t i = 0; i < scope.sampleCount; i++) {
                histogram.AddSample(scope.samples[i].gpuElapsed);
            }
            stats.gpu.p95 = histogram.GetPercentile(95);

            return stats;
        }

        void UpdateDrawHistogram(const Scope &scope, const Stats &stats) {
            if (drawHistogramIndex != lastDrawHistogramIndex || drawHistogramMode != lastDrawHistogramMode) {
                drawHistogram.max = 0;
                drawHistogram.min = std::numeric_limits<uint64>::max();
                lastDrawHistogramIndex = drawHistogramIndex;
                lastDrawHistogramMode = drawHistogramMode;
            }

            auto newMax = drawHistogramMode == Mode::CPU ? stats.cpu.p95 : stats.gpu.p95;
            newMax = CeilToPowerOfTwo(newMax);
            auto newMin = drawHistogramMode == Mode::CPU ? stats.cpu.min : stats.gpu.min;
            newMin = CeilToPowerOfTwo(newMin / 2);
            drawHistogram.Reset(std::min(drawHistogram.min, newMin), std::max(drawHistogram.max, newMax));
            for (size_t i = 0; i < scope.sampleCount; i++) {
                const auto &sample = scope.samples[i];
                drawHistogram.AddSample(drawHistogramMode == Mode::CPU ? sample.cpuElapsed : sample.gpuElapsed);
            }
        }

        PerfTimer &timer;

        Histogram<200> histogram;
        int msWindowSize;

        size_t drawHistogramIndex = 0, lastDrawHistogramIndex = 0;
        Mode drawHistogramMode = Mode::CPU, lastDrawHistogramMode = Mode::CPU;
        bool histogramLocked = false;
        Histogram<100> drawHistogram;

        chrono_clock::time_point lastWindowStart;

        size_t resultCount;
        vector<Scope> resultScopes;
    };
} // namespace sp::vulkan
