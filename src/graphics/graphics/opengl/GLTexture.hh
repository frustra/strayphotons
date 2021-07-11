#pragma once

#include "graphics/core/Texture.hh"
#include "graphics/opengl/Graphics.hh"
#include "graphics/opengl/PixelFormat.hh"

namespace sp {
    class Asset;

    struct GLTexture final : public GpuTexture {
        // Passing levels = FullyMipmap indicates storage
        // should be allocated for all possible downsamples.
        enum { FullyMipmap = -1 };

        GLuint handle = 0;
        GLenum target = 0;
        GLPixelFormat format;
        GLsizei width = 0, height = 0, depth = 0, levels = 0;
        bool assigned = false; // If true, do not destroy glTexture on dealloc

        // For color attachments, must be GL_COLOR_ATTACHMENT0.
        GLenum attachment;

        GLTexture &Create(GLenum target = GL_TEXTURE_2D);
        GLTexture &Assign(GLenum target, GLuint handle);
        GLTexture &Delete();

        void Bind(GLuint binding) const;
        void BindImage(GLuint binding,
                       GLenum access,
                       GLint level = 0,
                       GLboolean layered = false,
                       GLint layer = 0) const;
        void BindImageConvert(GLuint binding,
                              GLPixelFormat bindFormat,
                              GLenum access,
                              GLint level = 0,
                              GLboolean layered = false,
                              GLint layer = 0) const;
        void Clear(const void *data, GLint level = 0) const;

        GLTexture &Filter(GLenum minFilter = GL_LINEAR, GLenum magFilter = GL_LINEAR, float anisotropy = 0.0f);
        GLTexture &Wrap(GLenum wrapS = GL_CLAMP_TO_EDGE,
                        GLenum wrapT = GL_CLAMP_TO_EDGE,
                        GLenum wrapR = GL_CLAMP_TO_EDGE);
        GLTexture &BorderColor(glm::vec4 borderColor);
        GLTexture &Compare(GLenum mode = GL_COMPARE_REF_TO_TEXTURE, GLenum func = GL_LEQUAL);
        GLTexture &Size(GLsizei width, GLsizei height, GLsizei depth = 1);

        GLTexture &Storage(GLPixelFormat format, GLsizei levels = 1);
        GLTexture &Storage(PixelFormat format, GLsizei levels = 1);
        GLTexture &Storage(GLenum internalFormat,
                           GLenum format,
                           GLenum type,
                           GLsizei levels = 1,
                           bool preferSRGB = false);
        GLTexture &Image2D(const void *pixels,
                           GLint level = 0,
                           GLsizei subWidth = 0,
                           GLsizei subHeight = 0,
                           GLsizei xoffset = 0,
                           GLsizei yoffset = 0,
                           bool genMipmap = true);
        GLTexture &Image3D(const void *pixels,
                           GLint level = 0,
                           GLsizei subWidth = 0,
                           GLsizei subHeight = 0,
                           GLsizei subDepth = 0,
                           GLsizei xoffset = 0,
                           GLsizei yoffset = 0,
                           GLsizei zoffset = 0,
                           bool genMipmap = true);
        GLTexture &GenMipmap();

        GLTexture &LoadFromImage(shared_ptr<Image> image, GLsizei levels = FullyMipmap);
        GLTexture &Attachment(GLenum attachment);

        bool operator==(const GLTexture &other) const {
            return other.handle == handle;
        }

        bool operator!=(const GLTexture &other) const {
            return !(*this == other);
        }

    public:
        uintptr_t GetHandle() const override {
            return handle;
        }

        int GetWidth() const override {
            return width;
        }

        int GetHeight() const override {
            return height;
        }
    };

    struct BasicMaterial {
        GLTexture baseColorTex, metallicRoughnessTex, heightTex;

        BasicMaterial(unsigned char *baseColor = nullptr,
                      unsigned char *metallicRoughness = nullptr,
                      unsigned char *bump = nullptr) {
            unsigned char baseColorDefault[4] = {255, 255, 255, 255};
            unsigned char metallicRoughnessDefault[4] = {0, 255, 0, 0}; // Roughness = G channel, Metallic = B channel
            unsigned char bumpDefault[4] = {127, 127, 127, 255};

            if (!baseColor) baseColor = baseColorDefault;
            if (!metallicRoughness) metallicRoughness = metallicRoughnessDefault;
            if (!bump) bump = bumpDefault;

            baseColorTex.Create()
                .Filter(GL_NEAREST, GL_NEAREST)
                .Wrap(GL_REPEAT, GL_REPEAT)
                .Size(1, 1)
                .Storage(PF_RGB8)
                .Image2D(baseColor);

            metallicRoughnessTex.Create()
                .Filter(GL_NEAREST, GL_NEAREST)
                .Wrap(GL_REPEAT, GL_REPEAT)
                .Size(1, 1)
                .Storage(PF_R8)
                .Image2D(metallicRoughness);

            heightTex.Create()
                .Filter(GL_NEAREST, GL_NEAREST)
                .Wrap(GL_REPEAT, GL_REPEAT)
                .Size(1, 1)
                .Storage(PF_R8)
                .Image2D(bump);
        }
    };
} // namespace sp
