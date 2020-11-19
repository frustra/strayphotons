#pragma once

#include "Common.hh"
#include "RenderTarget.hh"
#include "core/Logging.hh"

#include <unordered_map>

namespace sp {
	enum { MaxFramebufferAttachments = 8 };

	struct RenderTargetHandle {
		GLuint tex = 0;
		GLuint buf = 0;

		RenderTargetHandle &operator=(const RenderTarget::Ref &other) {
			if (!other) return *this;
			if (other->GetDesc().renderBuffer) {
				buf = other->GetHandle();
			} else {
				tex = other->GetHandle();
			}
			return *this;
		}

		bool operator!() const {
			return tex == 0 && buf == 0;
		}

		bool operator==(const RenderTargetHandle &other) const {
			return tex == other.tex && buf == other.buf;
		}

		bool operator!=(const RenderTargetHandle &other) const {
			return !(*this == other);
		}

		bool operator==(const RenderTarget::Ref &other) const {
			if (!other) return tex == 0 && buf == 0;
			if (other->GetDesc().renderBuffer) { return tex == 0 && buf == other->GetHandle(); }
			return buf == 0 && tex == other->GetHandle();
		}

		bool operator!=(const RenderTarget::Ref &other) const {
			return !(*this == other);
		}
	};

	struct FramebufferState {
		uint32 NumAttachments;
		RenderTargetHandle Attachments[MaxFramebufferAttachments];
		RenderTargetHandle DepthStencilAttachment;

		FramebufferState(
			uint32 numAttachments, RenderTarget::Ref *attachments, RenderTarget::Ref depthStencilAttachment)
			: NumAttachments(numAttachments) {
			Assert(numAttachments <= MaxFramebufferAttachments, "exceeded maximum framebuffer attachment count");

			for (uint32 i = 0; i < numAttachments; i++) {
				this->Attachments[i] = attachments[i];
			}

			this->DepthStencilAttachment = depthStencilAttachment;
		}

		bool operator==(const FramebufferState &other) const {
			if (NumAttachments != other.NumAttachments) return false;

			if (DepthStencilAttachment != other.DepthStencilAttachment) return false;

			for (size_t i = 0; i < NumAttachments; i++) {
				if (Attachments[i] != other.Attachments[i]) return false;
			}

			return true;
		}
	};

	struct FramebufferStateHasher {
		size_t operator()(const FramebufferState &key) const {
			size_t hash = 0;

			hash_combine(hash, key.NumAttachments);

			hash_combine(hash, key.DepthStencilAttachment.tex);
			hash_combine(hash, key.DepthStencilAttachment.buf);

			for (size_t i = 0; i < key.NumAttachments; i++) {
				hash_combine(hash, key.Attachments[i].tex);
				hash_combine(hash, key.Attachments[i].buf);
			}

			return hash;
		}
	};

	class RenderTargetPool {
	public:
		RenderTarget::Ref Get(const RenderTargetDesc desc);
		void TickFrame();
		~RenderTargetPool();

		GLuint GetFramebuffer(
			uint32 numAttachments, RenderTarget::Ref *attachments, RenderTarget::Ref depthStencilAttachment);
		void FreeFramebuffersWithAttachment(RenderTarget::Ref attachment);

	private:
		vector<RenderTarget::Ref> pool;
		int64 nextRenderTargetID = 0;

		std::unordered_map<FramebufferState, GLuint, FramebufferStateHasher> framebufferCache;
	};
} // namespace sp
