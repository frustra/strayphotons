#include "graphics/RenderTarget.hh"

#include "core/Logging.hh"
#include "graphics/RenderTargetPool.hh"

namespace sp {
	void RenderTargetDesc::Prepare(
		RenderTargetPool *rtPool, shared_ptr<RenderTarget> &target, bool clear, const void *data) {
		if (!target || target->GetDesc() != *this) {
			target = rtPool->Get(*this);

			if (clear) {
				for (uint32 i = 0; i < this->levels; i++) { target->GetTexture().Clear(data, i); }
			}
		}
	}

	RenderTarget::RenderTarget(RenderTargetDesc desc) : desc(desc) {}

	RenderTarget::~RenderTarget() {
		tex.Delete();
	}
} // namespace sp
