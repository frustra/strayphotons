#pragma once

#include "Common.hh"
#include "core/Logging.hh"
#include "RenderTarget.hh"

#include <unordered_map>
#include <boost/functional/hash.hpp>

namespace sp
{
	enum { MaxFramebufferAttachments = 4 };

	struct FramebufferState
	{
		uint32 numAttachments;
		Texture attachments[MaxFramebufferAttachments];
		Texture depthStencilAttachment;

		FramebufferState(uint32 numAttachments, const Texture *attachments, Texture depthStencilAttachment)
			: numAttachments(numAttachments)
		{
			for (uint32 i = 0; i < numAttachments; i++)
			{
				this->attachments[i] = attachments[i];
			}

			this->depthStencilAttachment = depthStencilAttachment;
		}

		bool operator==(const FramebufferState &other) const
		{
			if (numAttachments != other.numAttachments)
				return false;

			if (depthStencilAttachment != other.depthStencilAttachment)
				return false;

			for (size_t i = 0; i < numAttachments; i++)
				if (attachments[i] != other.attachments[i])
					return false;

			return true;
		}
	};
}

namespace std
{
	template<>
	struct hash<sp::FramebufferState>
	{
		size_t operator()(const sp::FramebufferState &key) const
		{
			size_t hash = 0;

			boost::hash_combine(hash, key.numAttachments);
			boost::hash_combine(hash, key.depthStencilAttachment.handle);

			for (size_t i = 0; i < key.numAttachments; i++)
				boost::hash_combine(hash, key.attachments[i].handle);

			return hash;
		}
	};
}

namespace sp
{
	class RenderTargetPool
	{
	public:
		RenderTarget::Ref Get(const RenderTargetDesc &desc);
		void TickFrame();

		GLuint GetFramebuffer(uint32 numAttachments, const Texture *attachments, Texture depthStencilAttachment);
		void FreeFramebuffersWithAttachment(Texture attachment);

	private:
		vector<RenderTarget::Ref> pool;
		int64 nextRenderTargetID = 0;

		std::unordered_map<FramebufferState, GLuint> framebufferCache;
	};
}
