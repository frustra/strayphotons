#include "Shared.hh"
#include "core/Logging.hh"

#ifdef _WIN32
#include <intrin.h>
#define os_break __debugbreak
#else
#define os_break __builtin_trap
#endif

namespace sp
{
	void Assert(bool condition, string message)
	{
#ifndef NDEBUG
		if (!condition)
		{
			Errorf("assertion failed: %s", message.c_str());
			os_break();
			throw std::runtime_error(message);
		}
#endif
	}

	void Assert(bool condition)
	{
		return Assert(condition, "assertion failed");
	}
}
