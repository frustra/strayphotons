#include "Common.hh"
#include "core/Logging.hh"

#ifdef _WIN32
#include <intrin.h>
#define os_break __debugbreak
#else
#define os_break __builtin_trap
#endif

namespace sp
{
	void Assert(bool condition, const string &message)
	{
#ifndef NDEBUG
		if (!condition)
		{
			Errorf("assertion failed: %s", message);
			os_break();
			throw std::runtime_error(message);
		}
#endif
	}

	void Assert(bool condition)
	{
		static const string message = "assertion failed";
		return Assert(condition, message);
	}

	uint32 CeilToPowerOfTwo(uint32 v)
	{
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;
		return v;
	}

	uint32 Uint32Log2(uint32 v)
	{
		uint32 r = 0;
		while (v >>= 1) r++;
		return r;
	}
}
