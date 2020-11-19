#pragma once

#include "Common.hh"
#include "RenderBuffer.hh"
#include "Texture.hh"

#include <cstring>
#include <glm/glm.hpp>

namespace sp {
	class RenderTarget;
	class RenderTargetPool;

	namespace xr {
		class XrCompositor;
	}

	struct RenderTargetDesc {
		RenderTargetDesc() {}

		RenderTargetDesc(PixelFormat format, glm::ivec3 extent)
			: format(format), extent(extent), attachment(GL_COLOR_ATTACHMENT0) {
			auto glformat = GLPixelFormat::PixelFormatMapping(format).format;

			// Try to deduce attachment point.
			// The attachment can be controlled manually via the other constructor.
			if (glformat == GL_DEPTH_COMPONENT) {
				attachment = GL_DEPTH_ATTACHMENT;
			} else if (glformat == GL_DEPTH_STENCIL) {
				attachment = GL_DEPTH_STENCIL_ATTACHMENT;
			} else if (glformat == GL_STENCIL_INDEX) {
				attachment = GL_STENCIL_ATTACHMENT;
			}
		}

		RenderTargetDesc(PixelFormat format, glm::ivec2 extent)
			: RenderTargetDesc(format, glm::ivec3(extent.x, extent.y, 1)) {}

		RenderTargetDesc(PixelFormat format, glm::ivec2 extent, GLenum attachment)
			: format(format), extent(extent.x, extent.y, 1), attachment(attachment) {}

		RenderTargetDesc(PixelFormat format, glm::ivec2 extent, bool renderBuffer)
			: RenderTargetDesc(format, glm::ivec3(extent.x, extent.y, 1)) {
			this->renderBuffer = renderBuffer;
		}

		RenderTargetDesc &Filter(GLenum minFilter, GLenum magFilter) {
			this->minFilter = minFilter;
			this->magFilter = magFilter;
			return *this;
		}

		RenderTargetDesc &Wrap(GLenum wrapS, GLenum wrapT, GLenum wrapR) {
			this->wrapS = wrapS;
			this->wrapT = wrapT;
			this->wrapR = wrapR;
			return *this;
		}

		RenderTargetDesc &Wrap(GLenum wrap) {
			this->wrapS = wrap;
			this->wrapT = wrap;
			this->wrapR = wrap;
			return *this;
		}

		void Prepare(
			RenderTargetPool *rtPool, shared_ptr<RenderTarget> &target, bool clear = false, const void *data = nullptr);

		PixelFormat format;
		glm::ivec3 extent = {0, 0, 0};
		uint32 levels = 1;
		bool depthCompare = false;
		bool multiSample = false;
		bool textureArray = false;
		bool renderBuffer = false;
		GLenum attachment;
		GLenum minFilter = GL_LINEAR_MIPMAP_LINEAR, magFilter = GL_LINEAR;
		GLenum wrapS = GL_CLAMP_TO_EDGE, wrapT = GL_CLAMP_TO_EDGE, wrapR = GL_CLAMP_TO_EDGE;
		glm::vec4 borderColor = {0.0f, 0.0f, 0.0f, 0.0f};
		float anisotropy = 0.0;

		bool operator==(const RenderTargetDesc &other) const {
			return std::memcmp(this, &other, sizeof(RenderTargetDesc)) == 0;
		}

		bool operator!=(const RenderTargetDesc &other) const {
			return !(*this == other);
		}
	};

	class RenderTarget {
	public:
		typedef shared_ptr<RenderTarget> Ref;

		RenderTarget(RenderTargetDesc desc);
		~RenderTarget();

		Texture &GetTexture() {
			Assert(tex.handle, "target is a renderbuffer");
			return tex;
		}

		RenderBuffer &GetRenderBuffer() {
			Assert(buf.handle, "target is a texture");
			return buf;
		}

		GLuint GetHandle() {
			Assert(tex.handle || buf.handle, "render target must have an underlying target");
			if (tex.handle) return tex.handle;
			return buf.handle;
		}

		RenderTargetDesc GetDesc() {
			return desc;
		}

		bool operator==(const RenderTarget &other) const {
			return other.desc == desc && other.tex == tex && other.buf == buf;
		}

		bool operator!=(const RenderTarget &other) const {
			return !(*this == other);
		}

	private:
		RenderTargetDesc desc;
		Texture tex;
		RenderBuffer buf;

		int unusedFrames = 0;
		friend class RenderTargetPool;
		friend class xr::XrCompositor;
	};
} // namespace sp
