#include "graphics/RenderTarget.hh"
#include "core/Logging.hh"

namespace sp
{
	RenderTarget::RenderTarget(RenderTargetDesc desc)
		: desc(desc)
	{
	}

	RenderTarget::~RenderTarget()
	{
		Debugf("Render target %d destroyed", id);
		tex.Delete();
		id = -1;
	}
}
