#pragma once

#include "core/PerfTimer.hh"

#include <game/gui/GuiManager.hh>
#include <imgui/imgui.h>
#include <sstream>

namespace sp {
	class ProfilerGui : public GuiRenderable {
	public:
		static const uint64 numFrameTimes = 32, sampleFrameTimeEvery = 10;

		ProfilerGui(PerfTimer *timer) : timer(timer), cpuFrameTimes(), gpuFrameTimes() {}
		virtual ~ProfilerGui() {}

		void Add() {
			if (timer->lastCompleteFrame.results.empty())
				return;
			if (CVarProfileCPU.Get() != 1 && CVarProfileGPU.Get() != 1)
				return;

			ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
			frameCount++;

			if (CVarProfileCPU.Get() == 1) {
				ImGui::Begin("CpuProfiler", nullptr, flags);

				if (frameCount % sampleFrameTimeEvery == 1) {
					auto root = timer->lastCompleteFrame.results[0];

					memmove(cpuFrameTimes, cpuFrameTimes + 1, (numFrameTimes - 1) * sizeof(*cpuFrameTimes));
					cpuFrameTimes[numFrameTimes - 1] =
						(float)std::chrono::duration_cast<std::chrono::milliseconds>(root.cpuElapsed).count();
				}

				ImGui::PlotLines("##frameTimes", cpuFrameTimes, numFrameTimes);
				AddResults(timer->lastCompleteFrame.results, false);

				ImGui::End();
			}

			if (CVarProfileGPU.Get() == 1) {
				ImGui::Begin("GpuProfiler", nullptr, flags);

				if (frameCount % sampleFrameTimeEvery == 1) {
					auto root = timer->lastCompleteFrame.results[0];
					double elapsed = (double)root.gpuElapsed / 1000000.0;

					memmove(gpuFrameTimes, gpuFrameTimes + 1, (numFrameTimes - 1) * sizeof(*gpuFrameTimes));
					gpuFrameTimes[numFrameTimes - 1] = (float)elapsed;
				}

				ImGui::PlotLines("##frameTimes", gpuFrameTimes, numFrameTimes);
				AddResults(timer->lastCompleteFrame.results, true);

				ImGui::End();
			}
		}

	private:
		size_t AddResults(const vector<TimeResult> &results, bool gpuTime, size_t offset = 0, int depth = 1) {
			while (offset < results.size()) {
				auto result = results[offset++];

				if (result.depth < depth)
					return offset - 1;

				if (result.depth > depth)
					continue;

				ImGui::PushID(offset);

				int depth = result.depth;
				double elapsed = gpuTime
									 ? (double)result.gpuElapsed / 1000000.0
									 : std::chrono::duration_cast<std::chrono::milliseconds>(result.cpuElapsed).count();

				if (ImGui::TreeNodeEx("node",
						ImGuiTreeNodeFlags_DefaultOpen,
						"%s %.2fms",
						result.name.c_str(),
						elapsed)) {
					offset = AddResults(results, gpuTime, offset, depth + 1);
					ImGui::TreePop();
				}

				ImGui::PopID();
			}
			return offset;
		}

		PerfTimer *timer;
		float cpuFrameTimes[numFrameTimes];
		float gpuFrameTimes[numFrameTimes];
		uint64 frameCount = 0;
	};
} // namespace sp