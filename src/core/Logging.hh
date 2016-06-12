#pragma once

//#define SP_VERBOSE_LOGGING

#include <string>
#include <cstring>
#include <iostream>
#include <boost/format.hpp>

#define Debugf(...) sp::logging::Debug(__FILE__, __LINE__, __VA_ARGS__)
#define Logf(...) sp::logging::Log(__FILE__, __LINE__, __VA_ARGS__)
#define Errorf(...) sp::logging::Error(__FILE__, __LINE__, __VA_ARGS__)

namespace sp
{
	namespace logging
	{
		enum class Level
		{
			Error,
			Log,
			Debug
		};

		void GlobalLogOutput(Level lvl, const std::string &line);

		inline static const char *basename(const char *file)
		{
			const char *r;
			if ((r = strrchr(file, '/'))) return r + 1;
			if ((r = strrchr(file, '\\'))) return r + 1;
			return file;
		}

		inline static void writeFormatter(Level lvl, boost::format &fmter)
		{
			auto str = fmter.str();
			std::cerr << str;
			GlobalLogOutput(lvl, str);
		}

		template <typename T1, typename... T>
		inline static void writeFormatter(Level lvl, boost::format &fmter, T1 t1, T... t)
		{
			fmter % t1;
			writeFormatter(lvl, fmter, t...);
		}

		inline static void writeLog(Level lvl, const char *file, int line, const std::string &message)
		{
#ifdef SP_VERBOSE_LOGGING
			boost::format fmter("%s  (%s:%d)\n");
			writeFormatter(lvl, fmter, message, basename(file), line);
#else
			boost::format fmter("%s\n");
			writeFormatter(lvl, fmter, message);
#endif
		}

		template <typename... T>
		inline static void writeLog(Level lvl, const char *file, int line, const std::string &fmt, T... t)
		{
#ifdef SP_VERBOSE_LOGGING
			boost::format fmter(fmt + "  (%s:%d)\n");
			writeFormatter(lvl, fmter, t..., basename(file), line);
#else
			boost::format fmter(fmt + "\n");
			writeFormatter(lvl, fmter, t...);
#endif
		}

		template <typename... T>
		static void Log(const char *file, int line, const std::string &fmt, T... t)
		{
			writeLog(Level::Log, file, line, "[log] " + fmt, t...);
		}

		template <typename... T>
		static void Debug(const char *file, int line, const std::string &fmt, T... t)
		{
			writeLog(Level::Debug, file, line, "[dbg] " + fmt, t...);
		}

		template <typename... T>
		static void Error(const char *file, int line, const std::string &fmt, T... t)
		{
			writeLog(Level::Error, file, line, "[err] " + fmt, t...);
		}
	}
}
