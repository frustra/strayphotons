#include "GLRenderTarget.hh"

#include "core/Logging.hh"
#include "graphics/opengl/RenderTargetPool.hh"

namespace sp {
    void RenderTargetDesc::Prepare(RenderTargetPool *rtPool,
                                   std::shared_ptr<GLRenderTarget> &target,
                                   bool clear,
                                   const void *data) {
        if (!target || target->GetDesc() != *this) {
            target = rtPool->Get(*this);

            if (clear) {
                for (uint32 i = 0; i < this->levels; i++) {
                    target->GetTexture().Clear(data, i);
                }
            }
        }
    }

    GLRenderTarget::GLRenderTarget(RenderTargetDesc desc) : desc(desc) {}

    GLRenderTarget::~GLRenderTarget() {
        tex.Delete();
    }
} // namespace sp
