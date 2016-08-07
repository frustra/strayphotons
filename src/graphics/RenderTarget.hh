#pragma once

#include "Common.hh"
#include "Texture.hh"
#include <glm/glm.hpp>

namespace sp
{
	struct RenderTargetDesc
	{
		RenderTargetDesc() {}

		RenderTargetDesc(PixelFormat format, glm::ivec3 extent) :
			format(format), extent(extent), attachment(GL_COLOR_ATTACHMENT0)
		{
			auto glformat = GLPixelFormat::PixelFormatMapping(format).format;

			// Try to deduce attachment point.
			// The attachment can be controlled manually via the other constructor.
			if (glformat == GL_DEPTH_COMPONENT)
			{
				attachment = GL_DEPTH_ATTACHMENT;
			}
			else if (glformat == GL_DEPTH_STENCIL)
			{
				attachment = GL_DEPTH_STENCIL_ATTACHMENT;
			}
			else if (glformat == GL_STENCIL_INDEX)
			{
				attachment = GL_STENCIL_ATTACHMENT;
			}
		}

		RenderTargetDesc(PixelFormat format, glm::ivec2 extent) :
			RenderTargetDesc(format, {extent.x, extent.y, 1}) {}

		RenderTargetDesc(PixelFormat format, glm::ivec2 extent, GLenum attachment) :
			format(format), extent(extent.x, extent.y, 1), attachment(attachment) {}

		PixelFormat format;
		glm::ivec3 extent = { 0, 0, 0 };
		uint32 levels = 1;
		GLenum attachment;

		bool operator==(const RenderTargetDesc &other) const
		{
			return other.format == format
				   && other.extent == extent
				   && other.levels == levels
				   && other.attachment == attachment;
		}

		bool operator!=(const RenderTargetDesc &other) const
		{
			return !(*this == other);
		}

	};

	class RenderTarget
	{
	public:
		typedef shared_ptr<RenderTarget> Ref;

		RenderTarget(RenderTargetDesc desc);
		~RenderTarget();

		int64 GetID()
		{
			return id;
		}

		const Texture &GetTexture()
		{
			return tex;
		}

		RenderTargetDesc GetDesc()
		{
			return desc;
		}

	private:
		RenderTargetDesc desc;
		int64 id;
		Texture tex;

		int unusedFrames = 0;
		friend class RenderTargetPool;
	};
}
