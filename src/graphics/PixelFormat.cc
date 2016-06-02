#include "graphics/PixelFormat.hh"

namespace sp
{
#define PF_DEF(name, format, layout, type) case name: return GLPixelFormat{format, layout, type};

	const GLPixelFormat GLPixelFormat::PixelFormatMapping(PixelFormat in)
	{
		switch (in)
		{
				PF_DEFINITIONS
		}
		Assert(false, "invalid PixelFormat");
	};

#undef PF_DEF
}
