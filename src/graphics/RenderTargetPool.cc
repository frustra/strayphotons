#include "graphics/RenderTargetPool.hh"

#include "core/Logging.hh"

#include <algorithm>

namespace sp {
	RenderTarget::Ref RenderTargetPool::Get(const RenderTargetDesc desc) {
		for (size_t i = 0; i < pool.size(); i++) {
			auto elem = pool[i];

			if (elem.use_count() <= 2 && elem->desc == desc) {
				elem->unusedFrames = 0;
				return elem;
			}
		}

		auto ptr = make_shared<RenderTarget>(desc);

		if (desc.renderBuffer) {
			Assert(desc.extent.z == 1, "renderbuffers can't be 3D");

			ptr->buf.Create().Size(desc.extent.x, desc.extent.y).Storage(desc.format).Attachment(desc.attachment);
		} else {
			if (desc.multiSample) {
				Assert(desc.extent.z == 1, "only 2D textures can be multisampled");
				ptr->tex.Create(GL_TEXTURE_2D_MULTISAMPLE);
			} else if (desc.textureArray) {
				ptr->tex.Create(GL_TEXTURE_2D_ARRAY);
			} else {
				ptr->tex.Create(desc.extent.z != 1 ? GL_TEXTURE_3D : GL_TEXTURE_2D);
			}

			ptr->tex.Filter(desc.minFilter, desc.magFilter, desc.anisotropy)
				.Wrap(desc.wrapS, desc.wrapT, desc.wrapR)
				.BorderColor(desc.borderColor)
				.Size(desc.extent.x, desc.extent.y, desc.extent.z)
				.Storage(desc.format, desc.levels)
				.Attachment(desc.attachment);

			if (desc.depthCompare)
				ptr->tex.Compare();
		}

		pool.push_back(ptr);
		return ptr;
	}

	void RenderTargetPool::TickFrame() {
		for (size_t i = 0; i < pool.size();) {
			auto elem = pool[i];
			bool removed = false;

			if (elem.use_count() <= 2) {
				// No external references.
				if (elem->unusedFrames++ > 4) {
					// Swap with back.
					if (i < pool.size() - 1) {
						pool[i] = std::move(pool.back());
					}
					pool.pop_back();
					removed = true;
					FreeFramebuffersWithAttachment(elem);
				}
			} else {
				elem->unusedFrames = 0;
			}

			if (!removed) {
				i++;
			}
		}
	}

	RenderTargetPool::~RenderTargetPool() {
		for (auto it = framebufferCache.begin(); it != framebufferCache.end();) {
			glDeleteFramebuffers(1, &it->second);
			it = framebufferCache.erase(it);
		}
	}

	GLuint RenderTargetPool::GetFramebuffer(
		uint32 numAttachments, RenderTarget::Ref *attachments, RenderTarget::Ref depthStencilAttachment) {
		FramebufferState key(numAttachments, attachments, depthStencilAttachment);

		auto cached = framebufferCache.find(key);
		if (cached != framebufferCache.end()) {
			return cached->second;
		}

		Debugf("Creating new framebuffer");

		GLuint newFB;
		glCreateFramebuffers(1, &newFB);

		for (uint32 i = 0; i < numAttachments; i++) {
			auto tex = attachments[i]->GetTexture();
			Assert(tex.attachment == GL_COLOR_ATTACHMENT0, "attachment is not a color attachment");
			glNamedFramebufferTexture(newFB, GL_COLOR_ATTACHMENT0 + i, tex.handle, 0);
		}

		if (depthStencilAttachment) {
			if (depthStencilAttachment->GetDesc().renderBuffer) {
				auto buf = depthStencilAttachment->GetRenderBuffer();
				auto atc = buf.attachment;
				Assert(atc == GL_DEPTH_ATTACHMENT || atc == GL_STENCIL_ATTACHMENT || atc == GL_DEPTH_STENCIL_ATTACHMENT,
					"attachment is not a depth attachment");
				glNamedFramebufferRenderbuffer(newFB, atc, GL_RENDERBUFFER, buf.handle);
			} else {
				auto tex = depthStencilAttachment->GetTexture();
				auto atc = tex.attachment;
				Assert(atc == GL_DEPTH_ATTACHMENT || atc == GL_STENCIL_ATTACHMENT || atc == GL_DEPTH_STENCIL_ATTACHMENT,
					"attachment is not a depth attachment");
				glNamedFramebufferTexture(newFB, atc, tex.handle, 0);
			}
		}

		static const GLenum availableAttachments[] = {GL_COLOR_ATTACHMENT0,
			GL_COLOR_ATTACHMENT1,
			GL_COLOR_ATTACHMENT2,
			GL_COLOR_ATTACHMENT3,
			GL_COLOR_ATTACHMENT4,
			GL_COLOR_ATTACHMENT5,
			GL_COLOR_ATTACHMENT6,
			GL_COLOR_ATTACHMENT7};

		glNamedFramebufferDrawBuffers(newFB, numAttachments, availableAttachments);

		framebufferCache[key] = newFB;
		return newFB;
	}

	void RenderTargetPool::FreeFramebuffersWithAttachment(RenderTarget::Ref attachment) {
		for (auto it = framebufferCache.begin(); it != framebufferCache.end();) {
			bool found = false;
			auto &key = it->first;

			if (key.DepthStencilAttachment == attachment) {
				found = true;
			} else if (!attachment->GetDesc().renderBuffer) {
				for (uint32 i = 0; i < key.NumAttachments; i++) {
					if (key.Attachments[i] == attachment) {
						found = true;
						break;
					}
				}
			}

			if (found) {
				glDeleteFramebuffers(1, &it->second);
				it = framebufferCache.erase(it);
			} else {
				it++;
			}
		}
	}
} // namespace sp
