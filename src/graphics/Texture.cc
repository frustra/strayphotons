#include "Texture.hh"
#include "assets/Asset.hh"

#include <stb_image.h>

namespace sp
{
	Texture &Texture::Create(GLenum target)
	{
		Assert(!handle);
		glCreateTextures(target, 1, &handle);
		return Filter().Wrap();
	}

	Texture &Texture::Delete()
	{
		if (handle)
			glDeleteTextures(1, &handle);
		handle = 0;
		return *this;
	}

	void Texture::Bind(GLuint binding) const
	{
		Assert(handle);
		glBindTextures(binding, 1, &handle);
	}

	void Texture::BindImage(GLuint binding, GLenum access, GLint level, GLboolean layered, GLint layer) const
	{
		Assert(handle);
		Assert(format.Valid());
		glBindImageTexture(binding, handle, level, layered, layer, access, format.internalFormat);
	}

	Texture &Texture::Filter(GLenum minFilter, GLenum magFilter, float anisotropy)
	{
		Assert(handle);
		glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, minFilter);
		glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, magFilter);

		if (anisotropy > 0.0f)
			glTextureParameterf(handle, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);

		return *this;
	}

	Texture &Texture::Wrap(GLenum wrapS, GLenum wrapT)
	{
		Assert(handle);
		glTextureParameteri(handle, GL_TEXTURE_WRAP_S, wrapS);
		glTextureParameteri(handle, GL_TEXTURE_WRAP_T, wrapT);
		return *this;
	}

	Texture &Texture::Size(GLsizei width, GLsizei height)
	{
		this->width = width;
		this->height = height;
		return *this;
	}

	Texture &Texture::Storage2D(GLPixelFormat format, GLsizei levels)
	{
		Assert(handle);
		Assert(width && height);

		if (levels == FullyMipmap)
		{
			int maxdim = width > height ? width : height;
			levels = Uint32Log2(CeilToPowerOfTwo(maxdim)) + 1;
		}

		this->format = format;
		this->levels = levels;
		glTextureStorage2D(handle, levels, this->format.internalFormat, width, height);
		return *this;
	}

	Texture &Texture::Storage2D(PixelFormat format, GLsizei levels)
	{
		return Storage2D(GLPixelFormat::PixelFormatMapping(format), levels);
	}

	Texture &Texture::Storage2D(GLenum internalFormat, GLenum format, GLenum type, GLsizei levels, bool preferSRGB)
	{
		return Storage2D(GLPixelFormat(internalFormat, format, type, preferSRGB), levels);
	}

	Texture &Texture::Image2D(const void *pixels, GLint level, GLsizei subWidth, GLsizei subHeight, GLsizei xoffset, GLsizei yoffset)
	{
		Assert(handle);
		Assert(pixels);
		Assert(width && height);
		Assert(level < levels);
		Assert(format.Valid());

		if (subWidth == 0) subWidth = width;
		if (subHeight == 0) subHeight = height;

		glTextureSubImage2D(handle, level, xoffset, yoffset, subWidth, subHeight, format.format, format.type, pixels);

		if (levels > 1 && level == 0)
			glGenerateTextureMipmap(handle);

		return *this;
	}

	Texture &Texture::LoadFromAsset(shared_ptr<Asset> asset, GLsizei levels)
	{
		Assert(handle);
		Assert(asset != nullptr);

		int w, h, comp;
		uint8 *data = stbi_load_from_memory(asset->Buffer(), asset->Size(), &w, &h, &comp, 0);

		Assert(data, "unknown image format");
		Assert(w > 0 && h > 0, "unknown image format");

		Size(w, h);

		PixelFormat fmt = comp == 1 ? PF_R8 : comp == 2 ? PF_RG8 : comp == 3 ? PF_RGB8 : comp == 4 ? PF_RGBA8 : PF_INVALID;
		Storage2D(fmt, levels);
		Image2D(data);
		stbi_image_free(data);

		return *this;
	}
}
