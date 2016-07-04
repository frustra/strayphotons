#pragma once

#include "Common.hh"
#include "core/Logging.hh"
#include "RenderTarget.hh"

#include <boost/unordered_map.hpp>
#include <boost/functional/hash.hpp>

namespace sp
{
	enum { MaxFramebufferAttachments = 4 };

	struct FramebufferState
	{
		uint32 NumAttachments;
		Texture Attachments[MaxFramebufferAttachments];
		const Texture *DepthStencilAttachment;

		FramebufferState(uint32 numAttachments, const Texture *attachments, const Texture *depthStencilAttachment)
			: NumAttachments(numAttachments)
		{
			for (uint32 i = 0; i < numAttachments; i++)
			{
				this->Attachments[i] = attachments[i];
			}

			this->DepthStencilAttachment = depthStencilAttachment;
		}

		bool operator==(const FramebufferState &other) const
		{
			if (NumAttachments != other.NumAttachments)
				return false;

			if (!DepthStencilAttachment != !other.DepthStencilAttachment)
				return false;

			if (DepthStencilAttachment && *DepthStencilAttachment != *other.DepthStencilAttachment)
				return false;

			for (size_t i = 0; i < NumAttachments; i++)
				if (Attachments[i] != other.Attachments[i])
					return false;

			return true;
		}
	};

	struct FramebufferStateHasher
	{
		size_t operator()(const FramebufferState &key) const
		{
			size_t hash = 0;

			boost::hash_combine(hash, key.NumAttachments);

			if (key.DepthStencilAttachment)
				boost::hash_combine(hash, key.DepthStencilAttachment->handle);

			for (size_t i = 0; i < key.NumAttachments; i++)
				boost::hash_combine(hash, key.Attachments[i].handle);

			return hash;
		}
	};

	class RenderTargetPool
	{
	public:
		RenderTarget::Ref Get(const RenderTargetDesc &desc);
		void TickFrame();

		GLuint GetFramebuffer(uint32 numAttachments, const Texture *attachments, const Texture *depthStencilAttachment);
		void FreeFramebuffersWithAttachment(Texture attachment);

	private:
		vector<RenderTarget::Ref> pool;
		int64 nextRenderTargetID = 0;

		boost::unordered_map<FramebufferState, GLuint, FramebufferStateHasher> framebufferCache;
	};
}
