#pragma once

#include "Common.hh"
#include "Graphics.hh"
#include "Texture.hh"
#include <glm/glm.hpp>

namespace sp
{
	struct RenderTargetDesc
	{
		RenderTargetDesc() {};

		glm::ivec2 extent = { 0, 0 };
		PixelFormat format;
	};

	class RenderTarget
	{
	public:
		typedef shared_ptr<RenderTarget> Ref;

	private:
		Texture tex;
	};

	class RenderTargetPool
	{
	public:
		bool Get(const RenderTargetDesc &desc);

	private:
		vector<RenderTarget::Ref> pool;
	};
}