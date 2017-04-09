#pragma once

#include "Graphics.hh"
#include "PixelFormat.hh"

namespace sp
{
	struct RenderBuffer
	{
		GLuint handle = 0;
		GLPixelFormat format;
		GLsizei width = 0, height = 0;

		// For color attachments, must be GL_COLOR_ATTACHMENT0.
		GLenum attachment;

		RenderBuffer &Create();
		RenderBuffer &Delete();

		RenderBuffer &Size(GLsizei width, GLsizei height);

		RenderBuffer &Storage(GLPixelFormat format);
		RenderBuffer &Storage(PixelFormat format);
		RenderBuffer &Storage(GLenum internalFormat, bool preferSRGB = false);

		RenderBuffer &Attachment(GLenum attachment);

		bool operator==(const RenderBuffer &other) const
		{
			return other.handle == handle;
		}

		bool operator!=(const RenderBuffer &other) const
		{
			return !(*this == other);
		}
	};
}