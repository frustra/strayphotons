#include "GPUTimer.hh"
#include "core/Logging.hh"

namespace sp
{
	CVar<bool> CVarProfileGPU("r.ProfileGPU", false, "Display GPU render timing");

	RenderPhase::RenderPhase(const string &name, GPUTimer *timer)
		: name(name)
	{
		Assert(timer);
		if (timer->Active())
		{
			timer->Register(*this);
			this->timer = timer;
		}
	}

	RenderPhase::~RenderPhase()
	{
		if (timer)
		{
			timer->Complete(*this);
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
		Assert(stack.top() == &phase.query);
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
			Assert(!pendingFrames.empty());

			auto &frame = pendingFrames.front();

			auto &result = frame.results[front.resultIndex];
			result.start = start;
			result.end = end;
			result.elapsed = end - start;

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