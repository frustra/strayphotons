#include "GPUTimer.hh"
#include "core/Logging.hh"

namespace sp
{
	CVar<bool> CVarProfileGPU("r.ProfileGPU", false, "Display GPU render timing");

	RenderPhase::RenderPhase(const string &phaseName, GPUTimer *gpuTimer)
		: name(phaseName)
	{
		StartTimer(gpuTimer);
	}

	RenderPhase::~RenderPhase()
	{
		if (timer)
		{
			timer->Complete(*this);
		}
	}

	void RenderPhase::StartTimer(GPUTimer *gpuTimer)
	{
		if (!timer)
		{
			Assert(gpuTimer, "created RenderPhase with null timer");
			if (gpuTimer->Active())
			{
				gpuTimer->Register(*this);
				timer = gpuTimer;
			}
		}
	}

	void GPUTimer::StartFrame()
	{
		if (CVarProfileGPU.Get() == 1)
		{
			pendingFrames.push({});
			currentFrame = &pendingFrames.back();
		}
	}

	void GPUTimer::EndFrame()
	{
		currentFrame = nullptr;
		Tick();
	}

	void GPUTimer::Register(RenderPhase &phase)
	{
		currentFrame->remaining++;

		phase.query.resultIndex = currentFrame->results.size();

		GPUTimeResult result;
		result.name = phase.name;
		result.depth = stack.size() + 1;
		currentFrame->results.push_back(result);

		if (queryPool.size() < 2)
		{
			glGenQueries(2, phase.query.queries);
		}
		else
		{
			phase.query.queries[1] = queryPool.back();
			queryPool.pop_back();
			phase.query.queries[0] = queryPool.back();
			queryPool.pop_back();
		}

		glQueryCounter(phase.query.queries[0], GL_TIMESTAMP);
		stack.push(&phase.query);
	}

	void GPUTimer::Complete(RenderPhase &phase)
	{
		Assert(stack.top() == &phase.query, "RenderPhase query mismatch");
		stack.pop();
		glQueryCounter(phase.query.queries[1], GL_TIMESTAMP);
		pending.push(phase.query);
	}

	void GPUTimer::Tick()
	{
		GLint available;
		GLuint64 start, end;

		while (pending.size() > 0)
		{
			auto front = pending.front();
			glGetQueryObjectiv(front.queries[1], GL_QUERY_RESULT_AVAILABLE, &available);
			if (available == 0) break;

			glGetQueryObjectiv(front.queries[0], GL_QUERY_RESULT_AVAILABLE, &available);
			if (available == 0) break;

			glGetQueryObjectui64v(front.queries[0], GL_QUERY_RESULT, &start);
			glGetQueryObjectui64v(front.queries[1], GL_QUERY_RESULT, &end);

			if (start > end)
			{
				// Missed frame
				pending.pop();
				continue;
			}
			Assert(!pendingFrames.empty(), "pendingFrames is not empty");

			auto &frame = pendingFrames.front();

			auto &result = frame.results[front.resultIndex];
			result.start = start;
			result.end = end;
			GLuint64 elapsed = end - start;
			if (front.resultIndex < (int)lastCompleteFrame.results.size())
			{
				GLuint64 lastElapsed = lastCompleteFrame.results[front.resultIndex].elapsed;
				if (elapsed < lastElapsed)
				{
					result.elapsed = std::max(elapsed, lastElapsed * 99 / 100);
				}
				else
				{
					result.elapsed = elapsed;
				}
			}
			else
			{
				result.elapsed = elapsed;
			}

			queryPool.push_back(front.queries[0]);
			queryPool.push_back(front.queries[1]);

			if (--frame.remaining == 0)
			{
				// All results from frame are in.
				lastCompleteFrame = std::move(frame);
				pendingFrames.pop();
			}

			pending.pop();
		}
	}

	bool GPUTimer::Active()
	{
		return currentFrame != nullptr;
	}
}