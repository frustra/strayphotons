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
		inline static const char *basename(const char *file)
		{
			const char *r;
			if ((r = strrchr(file, '/'))) return r + 1;
			if ((r = strrchr(file, '\\'))) return r + 1;
			return file;
		}

		inline static void writeFormatter(boost::format &fmter)
		{
			std::cerr << fmter;
		}

		template <typename T1, typename... T>
		inline static void writeFormatter(boost::format &fmter, T1 t1, T... t)
		{
			fmter % t1;
			writeFormatter(fmter, t...);
		}

		inline static void writeLog(const char *file, int line, const std::string &message)
		{
#ifdef SP_VERBOSE_LOGGING
			boost::format fmter("%s  (%s:%d)\n");
			writeFormatter(fmter, message, basename(file), line);
#else
			boost::format fmter("%s\n");
			writeFormatter(fmter, message);
#endif
		}

		template <typename... T>
		inline static void writeLog(const char *file, int line, const std::string &fmt, T... t)
		{
#ifdef SP_VERBOSE_LOGGING
			boost::format fmter(fmt + "  (%s:%d)\n");
			writeFormatter(fmter, t..., basename(file), line);
#else
			boost::format fmter(fmt + "\n");
			writeFormatter(fmter, t...);
#endif
		}

		template <typename... T>
		static void Log(const char *file, int line, const std::string &fmt, T... t)
		{
			writeLog(file, line, "[log] " + fmt, t...);
		}

		template <typename... T>
		static void Debug(const char *file, int line, const std::string &fmt, T... t)
		{
			writeLog(file, line, "[dbg] " + fmt, t...);
		}

		template <typename... T>
		static void Error(const char *file, int line, const std::string &fmt, T... t)
		{
			writeLog(file, line, "[err] " + fmt, t...);
		}
	}
}
