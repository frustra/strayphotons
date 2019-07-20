#include "graphics/RenderTarget.hh"
#include "graphics/RenderTargetPool.hh"
#include "core/Logging.hh"

namespace sp
{
	void RenderTargetDesc::Prepare(RenderTargetPool *rtPool, shared_ptr<RenderTarget> &target, bool clear, const void *data)
	{
		if (!target || target->GetDesc() != *this)
		{
			target = rtPool->Get(*this);

			if (clear)
			{
				for (uint32 i = 0; i < this->levels; i++)
				{
					target->GetTexture().Clear(data, i);
				}
			}
		}
	}

	RenderTarget::RenderTarget(RenderTargetDesc desc) : desc(desc)
	{
	}

	RenderTarget::~RenderTarget()
	{
		Debugf("Render target %d destroyed", id);
		tex.Delete();
		id = -1;
	}
}
