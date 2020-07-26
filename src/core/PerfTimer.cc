#include "PerfTimer.hh"

#include "core/Logging.hh"

#include <Common.hh>

namespace sp {
	CVar<bool> CVarProfileCPU("r.ProfileCPU", false, "Display CPU frame timing");
	CVar<bool> CVarProfileGPU("r.ProfileGPU", false, "Display GPU render timing");

	RenderPhase::RenderPhase(const string &phaseName, PerfTimer &perfTimer) : name(phaseName) {
		StartTimer(perfTimer);
	}

	RenderPhase::~RenderPhase() {
		if (timer) {
			timer->Complete(*this);
		}
	}

	void RenderPhase::StartTimer(PerfTimer &perfTimer) {
		if (!timer) {
			if (perfTimer.Active()) {
				timer = &perfTimer;
				perfTimer.Register(*this);
			}
		}
	}

	void PerfTimer::StartFrame() {
		if (CVarProfileCPU.Get() == 1 || CVarProfileGPU.Get() == 1) {
			pendingFrames.push({});
			currentFrame = &pendingFrames.back();
		}
	}

	void PerfTimer::EndFrame() {
		currentFrame = nullptr;
		Tick();
	}

	void PerfTimer::Register(RenderPhase &phase) {
		currentFrame->remaining++;

		phase.query.resultIndex = currentFrame->results.size();

		TimeResult result;
		result.name = phase.name;
		result.depth = stack.size() + 1;
		currentFrame->results.push_back(result);

		// Register a GL timestamp query
		if (glQueryPool.size() < 2) {
			glGenQueries(2, phase.query.glQueries);
		} else {
			phase.query.glQueries[1] = glQueryPool.back();
			glQueryPool.pop_back();
			phase.query.glQueries[0] = glQueryPool.back();
			glQueryPool.pop_back();
		}

		glQueryCounter(phase.query.glQueries[0], GL_TIMESTAMP);
		stack.push(&phase.query);

		// Save CPU time as close to start of work as possible.
		phase.query.cpuStart = chrono_clock::now();
	}

	void PerfTimer::Complete(RenderPhase &phase) {
		// Save CPU time as close to end of work as possible.
		phase.query.cpuEnd = chrono_clock::now();

		Assert(stack.top() == &phase.query, "RenderPhase query mismatch");
		stack.pop();
		glQueryCounter(phase.query.glQueries[1], GL_TIMESTAMP);
		pending.push(phase.query);
	}

	void PerfTimer::Tick() {
		GLint available;
		GLuint64 gpuStart, gpuEnd;

		while (pending.size() > 0) {
			// Wait for the GL timestamp queries to complete
			auto front = pending.front();
			glGetQueryObjectiv(front.glQueries[1], GL_QUERY_RESULT_AVAILABLE, &available);
			if (available == 0)
				break;

			glGetQueryObjectiv(front.glQueries[0], GL_QUERY_RESULT_AVAILABLE, &available);
			if (available == 0)
				break;

			// Get the GPU start and end time from the completed queries
			glGetQueryObjectui64v(front.glQueries[0], GL_QUERY_RESULT, &gpuStart);
			glGetQueryObjectui64v(front.glQueries[1], GL_QUERY_RESULT, &gpuEnd);

			if (gpuStart > gpuEnd) {
				// Missed frame
				pending.pop();
				continue;
			}

			// Store the results and apply some basic filtering
			Assert(!pendingFrames.empty(), "pendingFrames is not empty");
			auto &frame = pendingFrames.front();
			auto &result = frame.results[front.resultIndex];

			result.cpuElapsed = front.cpuEnd - front.cpuStart;
			result.gpuElapsed = gpuEnd - gpuStart;

			if (front.resultIndex < (int)lastCompleteFrame.results.size()) {
				// Smooth out the graph by applying a high-watermark filter.
				auto lastCpuElapsed = lastCompleteFrame.results[front.resultIndex].cpuElapsed;
				GLuint64 lastGpuElapsed = lastCompleteFrame.results[front.resultIndex].gpuElapsed;
				if (result.cpuElapsed < lastCpuElapsed) {
					result.cpuElapsed =
						chrono_clock::duration(std::max(result.cpuElapsed.count(), lastCpuElapsed.count() * 99 / 100));
				}
				if (result.gpuElapsed < lastGpuElapsed) {
					result.gpuElapsed = std::max(result.gpuElapsed, lastGpuElapsed * 99 / 100);
				}
			}
			glQueryPool.push_back(front.glQueries[0]);
			glQueryPool.push_back(front.glQueries[1]);

			if (--frame.remaining == 0) {
				// All results from frame are in.
				lastCompleteFrame = std::move(frame);
				pendingFrames.pop();
			}

			pending.pop();
		}
	}

	bool PerfTimer::Active() {
		return currentFrame != nullptr;
	}
} // namespace sp