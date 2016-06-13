#pragma once

#include "Graphics.hh"
#include "PixelFormat.hh"

namespace sp
{
	class Asset;

	struct Texture
	{
		// Passing levels = FullyMipmap indicates storage
		// should be allocated for all possible downsamples.
		enum { FullyMipmap = -1 };

		GLuint handle = 0;
		GLPixelFormat format;
		GLsizei width = 0, height = 0, levels = 0;

		Texture &Create(GLenum target = GL_TEXTURE_2D);
		Texture &Delete();

		void Bind(GLuint binding) const;
		void BindImage(GLuint binding, GLenum access, GLint level = 0, GLboolean layered = false, GLint layer = 0) const;

		Texture &Filter(GLenum minFilter = GL_LINEAR, GLenum magFilter = GL_LINEAR, float anisotropy = 0.0f);
		Texture &Wrap(GLenum wrapS = GL_CLAMP_TO_EDGE, GLenum wrapT = GL_CLAMP_TO_EDGE);
		Texture &Size(GLsizei width, GLsizei height);
		Texture &Storage2D(GLPixelFormat format, GLsizei levels = 1);
		Texture &Storage2D(PixelFormat format, GLsizei levels = 1);
		Texture &Storage2D(GLenum internalFormat, GLenum format, GLenum type, GLsizei levels = 1, bool preferSRGB = false);
		Texture &Image2D(const void *pixels, GLint level = 0, GLsizei subWidth = 0, GLsizei subHeight = 0, GLsizei xoffset = 0, GLsizei yoffset = 0);
		Texture &LoadFromAsset(shared_ptr<Asset> asset, GLsizei levels = FullyMipmap);

		bool operator==(const Texture &other) const
		{
			return other.handle == handle;
		}

		bool operator!=(const Texture &other) const
		{
			return !(*this == other);
		}
	};
}