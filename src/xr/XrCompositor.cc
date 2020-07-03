#include "xr/XrCompositor.hh"

using namespace sp;
using namespace xr;

void XrCompositor::CreateRenderTargetTexture(RenderTarget::Ref renderTarget, const RenderTargetDesc desc) {
	if (desc.renderBuffer) {
		throw std::runtime_error("XrCompositor does not produce renderBuffers");
	} else {
		if (desc.multiSample) {
			throw std::runtime_error("XrCompositor does not produce multisampled texture buffers");
		} else if (desc.textureArray) {
			renderTarget->tex.Create(GL_TEXTURE_2D_ARRAY);
		} else {
			renderTarget->tex.Create(desc.extent.z != 1 ? GL_TEXTURE_3D : GL_TEXTURE_2D);
		}

		renderTarget->tex.Filter(desc.minFilter, desc.magFilter, desc.anisotropy)
			.Wrap(desc.wrapS, desc.wrapT, desc.wrapR)
			.BorderColor(desc.borderColor)
			.Size(desc.extent.x, desc.extent.y, desc.extent.z)
			.Storage(desc.format, desc.levels)
			.Attachment(desc.attachment);

		if (desc.depthCompare)
			renderTarget->tex.Compare();
	}
}