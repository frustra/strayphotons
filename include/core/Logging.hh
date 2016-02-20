//=== Copyright Frustra Software, all rights reserved ===//

#ifndef SP_LOGGING_H
#define SP_LOGGING_H

#define SP_VERBOSE_LOGGING

#include <string>
#include <cstdarg>
#include <iostream>

namespace sp
{
	namespace logging
	{
		static void writeLog(const std::string &fmt, ...)
		{
			va_list ap;
			va_start(ap, fmt);
			vprintf(fmt.c_str(), ap);
			va_end(ap);
		}

		template <typename... T> static void Log(const std::string &fmt, T... t)
		{
			writeLog("\x1b[34;1m[log]\x1b[0m " + fmt + "\n", t...);
		}

		template <typename... T> static void Debug(const std::string &fmt, T... t)
		{
			DebugContext(fmt, "unknown file", -1, t...);
		}

		template <typename... T> static void Error(const std::string &fmt, T... t)
		{
			writeLog("\x1b[31;1m[err]\x1b[0m " + fmt + "\n", t...);
		}

		template <typename... T> static void DebugContext(const std::string &fmt, const std::string &file, int line, T... t)
		{
			writeLog("[dbg] " + fmt + "\n", t...);
		}
	}
}

#define Dbg(a) sp::logging::DebugContext(a, __FILE__, __LINE__)

#endif

