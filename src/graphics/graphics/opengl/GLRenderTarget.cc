#include "GLRenderTarget.hh"

#include "core/Logging.hh"
#include "graphics/opengl/GlfwGraphicsContext.hh"
#include "graphics/opengl/RenderTargetPool.hh"

namespace sp {
    void RenderTargetDesc::Prepare(GlfwGraphicsContext &context,
                                   std::shared_ptr<GLRenderTarget> &target,
                                   bool clear,
                                   const void *data) {
        if (!target || target->GetDesc() != *this) {
            target = context.GetRenderTarget(*this);

            if (clear) {
                for (GLint i = 0; i < this->levels; i++) {
                    target->GetGLTexture().Clear(data, i);
                }
            }
        }
    }

    GLRenderTarget::GLRenderTarget(RenderTargetDesc desc) : desc(desc) {}

    GLRenderTarget::~GLRenderTarget() {
        tex.Delete();
    }
} // namespace sp
