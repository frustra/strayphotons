#pragma once

#include "Common.hh"
#include "graphics/Graphics.hh"
#include "CVar.hh"

#include <stack>
#include <queue>

namespace sp
{
	class PerfTimer;
	extern CVar<bool> CVarProfileCPU;
	extern CVar<bool> CVarProfileGPU;

	struct TimeResult
	{
		string name;
		int depth = 0;
		double cpuStart = 0, cpuEnd = 0, cpuElapsed = 0;
		uint64 gpuStart = 0, gpuEnd = 0, gpuElapsed = 0;
	};

	struct TimeQuery
	{
		double cpuStart, cpuEnd;
		GLuint glQueries[2];
		int resultIndex;
	};

	class FrameTiming
	{
	public:
		vector<TimeResult> results;
		size_t remaining = 0;
	};

	class RenderPhase
	{
	public:
		const string name;
		PerfTimer *timer = nullptr;
		TimeQuery query;

		// Create phase without starting the timer.
		RenderPhase(const string &phaseName) : name(phaseName) { };

		// Create phase and automatically start the timer.
		RenderPhase(const string &phaseName, PerfTimer &perfTimer);

		// Starts the timer if not already started.
		void StartTimer(PerfTimer &perfTimer);

		~RenderPhase();
	};

	class PerfTimer
	{
	public:
		void StartFrame();
		void EndFrame();

		void Register(RenderPhase &phase);
		void Complete(RenderPhase &phase);

		void Tick();
		bool Active();

		FrameTiming lastCompleteFrame;

	private:
		std::stack<TimeQuery *> stack;
		std::queue<TimeQuery> pending;
		vector<GLuint> glQueryPool;

		FrameTiming *currentFrame = nullptr;
		std::queue<FrameTiming> pendingFrames;
	};
}