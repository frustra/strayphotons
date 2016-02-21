//=== Copyright Frustra Software, all rights reserved ===//

#ifndef SP_LOGGING_H
#define SP_LOGGING_H

#define SP_VERBOSE_LOGGING

#include <string>
#include <cstdarg>
#include <cstring>

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

		inline static void writeLogVA(const char *fmt, ...)
		{
			va_list ap;
			va_start(ap, fmt);
			vprintf(fmt, ap);
			va_end(ap);
		}

		inline static void writeLog(const char *file, int line, const std::string &message)
		{
#ifdef SP_VERBOSE_LOGGING
			writeLogVA("%s  (%s:%d)\n", message.c_str(), basename(file), line);
#else
			writeLogVA("%s\n", message.c_str());
#endif
		}

		template <typename... T> inline static void writeLog(const char *file, int line, const std::string &fmt, T... t)
		{
#ifdef SP_VERBOSE_LOGGING
			std::string fullFmt = fmt + "  (%s:%d)\n";
			writeLogVA(fullFmt.c_str(), t..., basename(file), line);
#else
			std::string fullFmt = fmt + "\n";
			writeLogVA(fullFmt.c_str(), t...);
#endif
		}

		template <typename... T> static void Log(const char *file, int line, const std::string &fmt, T... t)
		{
			writeLog(file, line, "\x1b[34;1m[log]\x1b[0m " + fmt, t...);
		}

		template <typename... T> static void Debug(const char *file, int line, const std::string &fmt, T... t)
		{
			writeLog(file, line, "\x1b[33;1m[dbg]\x1b[0m " + fmt, t...);
		}

		template <typename... T> static void Error(const char *file, int line, const std::string &fmt, T... t)
		{
			writeLog(file, line, "\x1b[31;1m[err]\x1b[0m " + fmt, t...);
		}
	}
}

#endif

