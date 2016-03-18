//=== Copyright Frustra Software, all rights reserved ===//

#ifdef _WIN32
#include <intrin.h>
#define os_break __debugbreak
#else
#define os_break __builtin_trap
#endif

namespace sp
{
	void Assert(bool condition)
	{
#ifndef NDEBUG
		if (!condition)
		{
			os_break();
			throw "assertion failed";
		}
#endif
	}
}
