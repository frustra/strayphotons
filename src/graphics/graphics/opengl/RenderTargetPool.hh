#pragma once

#include "core/Common.hh"
#include "core/Hashing.hh"
#include "core/Logging.hh"
#include "graphics/opengl/GLRenderTarget.hh"

#include <unordered_map>

namespace sp {
    enum { MaxFramebufferAttachments = 8 };

    struct RenderTargetHandle {
        GLuint tex = 0;
        GLuint buf = 0;

        RenderTargetHandle &operator=(GLRenderTarget *other) {
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

        bool operator==(GLRenderTarget *&other) const {
            if (!other) return tex == 0 && buf == 0;
            if (other->GetDesc().renderBuffer) { return tex == 0 && buf == other->GetHandle(); }
            return buf == 0 && tex == other->GetHandle();
        }

        bool operator!=(GLRenderTarget *&other) const {
            return !(*this == other);
        }
    };

    struct FramebufferState {
        uint32 NumAttachments;
        RenderTargetHandle Attachments[MaxFramebufferAttachments];
        RenderTargetHandle DepthStencilAttachment;

        FramebufferState(uint32 numAttachments, GLRenderTarget **attachments, GLRenderTarget *depthStencilAttachment)
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
        uint64 operator()(const FramebufferState &key) const {
            uint64 hash = 0;

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
        std::shared_ptr<GLRenderTarget> Get(const RenderTargetDesc &desc);
        void TickFrame();
        ~RenderTargetPool();

        GLuint GetFramebuffer(uint32 numAttachments,
                              GLRenderTarget **attachments,
                              GLRenderTarget *depthStencilAttachment);
        void FreeFramebuffersWithAttachment(GLRenderTarget *attachment);

    private:
        std::vector<std::shared_ptr<GLRenderTarget>> pool;

        std::unordered_map<FramebufferState, GLuint, FramebufferStateHasher> framebufferCache;
    };
} // namespace sp
