#pragma once

#include "Common.hh"

#include <fmod_studio.hpp>
#include <fmod.hpp>
#include <boost/unordered_map.hpp>

namespace sp
{
	void FmodErrCheckFn(FMOD_RESULT res, const char *file, int line);

	class AudioManager
	{
	public:
		AudioManager();
		~AudioManager();

		// Set the audio driver. View logs on startup to see available drivers.
		bool SetDriver(int driverIndex);

		bool Frame();
		void LoadBank(const string &filename);

		void LoadProjectFiles();
		void StartEvent(const string &eventName);

	private:
		void logAvailDrivers();
		void initDriver();
		void initOutputType();

		FMOD::Studio::System *system = nullptr;
		FMOD::System *lowSystem = nullptr;
		vector<FMOD::Studio::Bank *> banks;
		boost::unordered_map<const string, FMOD::Studio::EventDescription *>
		eventDescriptions;

	};
}

#define FMOD_CHECK(fmodResult) sp::FmodErrCheckFn(fmodResult, __FILE__, __LINE__)
