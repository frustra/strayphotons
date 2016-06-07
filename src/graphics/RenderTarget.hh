#pragma once

#include "Common.hh"
#include "Texture.hh"
#include <glm/glm.hpp>

namespace sp
{
	struct RenderTargetDesc
	{
		RenderTargetDesc() {}
		RenderTargetDesc(PixelFormat format, glm::ivec2 extent) :
			format(format), extent(extent) {}

		PixelFormat format;
		glm::ivec2 extent = { 0, 0 };
		uint32 levels = 1;

		bool operator==(const RenderTargetDesc &other) const
		{
			return other.format == format
				   && other.extent == extent
				   && other.levels == levels;
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
