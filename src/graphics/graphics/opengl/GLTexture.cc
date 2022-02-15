#include "GLTexture.hh"

#include "assets/Asset.hh"
#include "assets/Image.hh"

#include <algorithm>

namespace sp {
    GLTexture &GLTexture::Create(GLenum target) {
        Assert(!handle, "texture cannot be recreated");
        this->target = target;
        glCreateTextures(target, 1, &handle);
        return Filter().Wrap();
    }

    GLTexture &GLTexture::Assign(GLenum target, GLuint handle) {
        Assert(!this->handle, "texture cannot be recreated");
        this->target = target;
        this->handle = handle;
        assigned = true;
        return *this;
    }

    GLTexture &GLTexture::Delete() {
        if (handle && !assigned) glDeleteTextures(1, &handle);
        handle = 0;
        target = 0;
        return *this;
    }

    void GLTexture::Bind(GLuint binding) const {
        Assert(handle, "null texture handle");
        glBindTextures(binding, 1, &handle);
    }

    void GLTexture::BindImage(GLuint binding, GLenum access, GLint level, GLboolean layered, GLint layer) const {
        Assert(handle, "null texture handle");
        Assert(format.Valid(), "binding texture without format specified");
        glBindImageTexture(binding, handle, level, layered, layer, access, format.internalFormat);
    }

    void GLTexture::BindImageConvert(GLuint binding,
        GLPixelFormat bindFormat,
        GLenum access,
        GLint level,
        GLboolean layered,
        GLint layer) const {
        Assert(handle, "null texture handle");
        Assert(format.Valid(), "binding texture without format specified");
        glBindImageTexture(binding, handle, level, layered, layer, access, bindFormat.internalFormat);
    }

    void GLTexture::Clear(const void *data, GLint level) const {
        glClearTexImage(handle, level, format.format, format.type, data);
    }

    GLTexture &GLTexture::Filter(GLenum minFilter, GLenum magFilter, float anisotropy) {
        Assert(handle, "null texture handle");
        if (target == GL_TEXTURE_2D_MULTISAMPLE) return *this;

        glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, minFilter);
        glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, magFilter);

        if (anisotropy > 0.0f) glTextureParameterf(handle, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);

        return *this;
    }

    GLTexture &GLTexture::Wrap(GLenum wrapS, GLenum wrapT, GLenum wrapR) {
        Assert(handle, "null texture handle");
        if (target == GL_TEXTURE_2D_MULTISAMPLE) return *this;

        glTextureParameteri(handle, GL_TEXTURE_WRAP_S, wrapS);
        glTextureParameteri(handle, GL_TEXTURE_WRAP_T, wrapT);
        if (target == GL_TEXTURE_3D) glTextureParameteri(handle, GL_TEXTURE_WRAP_R, wrapR);
        return *this;
    }

    GLTexture &GLTexture::BorderColor(glm::vec4 borderColor) {
        Assert(handle, "null texture handle");
        glTextureParameterfv(handle, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(borderColor));
        return *this;
    }

    GLTexture &GLTexture::Compare(GLenum mode, GLenum func) {
        Assert(handle, "null texture handle");
        glTextureParameteri(handle, GL_TEXTURE_COMPARE_MODE, mode);
        glTextureParameteri(handle, GL_TEXTURE_COMPARE_FUNC, func);
        return *this;
    }

    GLTexture &GLTexture::Size(GLsizei width, GLsizei height, GLsizei depth) {
        this->width = width;
        this->height = height;
        this->depth = depth;
        return *this;
    }

    int CalculateMipmapLevels(int width, int height, int depth) {
        int dim = std::max(std::max(width, height), depth);
        if (dim <= 0) return 1;
        int cmp = 31;
        while (!(dim >> cmp))
            cmp--;
        return cmp + 1;
    }

    GLTexture &GLTexture::Storage(GLPixelFormat format, GLsizei levels) {
        Assert(format.Valid(), "invalid texture format");
        Assert(handle, "null texture handle");
        Assert(width && height, "texture size must be set before storage format");
        Assert(!assigned, "do not call Storage() on assigned textures");

        if (levels == FullyMipmap) { levels = CalculateMipmapLevels(width, height, depth); }

        this->format = format;
        this->levels = levels;

        switch (target) {
        case GL_TEXTURE_1D_ARRAY:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_3D:
            glTextureStorage3D(handle, levels, this->format.internalFormat, width, height, depth);
            break;
        case GL_TEXTURE_2D_MULTISAMPLE:
            glTextureStorage2DMultisample(handle, 4, this->format.internalFormat, width, height, false);
            break;
        default:
            glTextureStorage2D(handle, levels, this->format.internalFormat, width, height);
        }

        return *this;
    }

    GLTexture &GLTexture::Storage(PixelFormat format, GLsizei levels) {
        return Storage(GLPixelFormat::PixelFormatMapping(format), levels);
    }

    GLTexture &GLTexture::Storage(GLenum internalFormat, GLenum format, GLenum type, GLsizei levels, bool preferSRGB) {
        return Storage(GLPixelFormat(internalFormat, format, type, preferSRGB), levels);
    }

    GLTexture &GLTexture::Image2D(const void *pixels,
        GLint level,
        GLsizei subWidth,
        GLsizei subHeight,
        GLsizei xoffset,
        GLsizei yoffset,
        bool genMipmap) {
        Assert(handle, "null texture handle");
        Assert(pixels, "null pixel data");
        Assert(width && height, "texture size must be set before data");
        Assert(level < levels, "setting texture data for invalid level");
        Assert(format.Valid(), "setting texture data without format specified");

        if (subWidth == 0) subWidth = width;
        if (subHeight == 0) subHeight = height;

        glTextureSubImage2D(handle, level, xoffset, yoffset, subWidth, subHeight, format.format, format.type, pixels);

        if (genMipmap && levels > 1 && level == 0) GenMipmap();

        return *this;
    }

    GLTexture &GLTexture::Image3D(const void *pixels,
        GLint level,
        GLsizei subWidth,
        GLsizei subHeight,
        GLsizei subDepth,
        GLsizei xoffset,
        GLsizei yoffset,
        GLsizei zoffset,
        bool genMipmap) {
        Assert(handle, "null texture handle");
        Assert(pixels, "null pixel data");
        Assert(width && height && depth, "texture size must be set before data");
        Assert(level < levels, "setting texture data for invalid level");
        Assert(format.Valid(), "setting texture data without format specified");

        if (subWidth == 0) subWidth = width;
        if (subHeight == 0) subHeight = height;

        glTextureSubImage3D(handle,
            level,
            xoffset,
            yoffset,
            zoffset,
            subWidth,
            subHeight,
            subDepth,
            format.format,
            format.type,
            pixels);

        if (genMipmap && level == 0) GenMipmap();

        return *this;
    }

    GLTexture &GLTexture::GenMipmap() {
        if (levels > 1) glGenerateTextureMipmap(handle);
        return *this;
    }

    GLTexture &GLTexture::LoadFromImage(std::shared_ptr<const Image> image, GLsizei levels) {
        Assert(handle, "null texture handle");
        Assert(image, "loading GLTexture from null image");

        int w = image->GetWidth();
        int h = image->GetHeight();
        int comp = image->GetComponents();
        const uint8_t *data = image->GetImage().get();

        Assert(data, "unknown image format");
        Assert(w > 0 && h > 0, "unknown image format");

        Size(w, h);

        switch (comp) {
        case 1:
            Storage(PF_R8, levels);
            break;
        case 2:
            Storage(PF_RG8, levels);
            break;
        case 3:
            Storage(PF_RGB8, levels);
            break;
        case 4:
            Storage(PF_RGBA8, levels);
            break;
        default:
            Storage(PF_INVALID, levels);
        }
        Image2D(data);

        return *this;
    }

    GLTexture &GLTexture::Attachment(GLenum attachment) {
        this->attachment = attachment;
        return *this;
    }
} // namespace sp
