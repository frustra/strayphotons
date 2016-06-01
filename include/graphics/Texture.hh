#pragma once

#include "Graphics.hh"
#include "PixelFormat.hh"

namespace sp
{
	struct Texture
	{
		GLuint handle = 0;
		PixelFormat format = PF_INVALID;

		void Delete()
		{
			glDeleteTextures(1, &handle);
		}

		void Bind(GLuint binding)
		{
			glBindTextures(binding, 1, &handle);
		}
	};

	// TODO(pushrax): stop being lazy
	static Texture NewTexture(
		GLenum target = GL_TEXTURE_2D,
		GLenum minFilter = GL_LINEAR,
		GLenum magFilter = GL_LINEAR,
		GLenum wrapS = GL_CLAMP_TO_EDGE,
		GLenum wrapT = GL_CLAMP_TO_EDGE)
	{
		Texture tex;
		glCreateTextures(target, 1, &tex.handle);
		glTextureParameteri(tex.handle, GL_TEXTURE_MIN_FILTER, minFilter);
		glTextureParameteri(tex.handle, GL_TEXTURE_MAG_FILTER, magFilter);
		glTextureParameteri(tex.handle, GL_TEXTURE_WRAP_S, wrapS);
		glTextureParameteri(tex.handle, GL_TEXTURE_WRAP_T, wrapT);
		return tex;
	}
}