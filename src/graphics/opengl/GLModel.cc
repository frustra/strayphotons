#include "graphics/opengl/GLModel.hh"

#include "core/Logging.hh"
#include "graphics/GenericShaders.hh"

// Ideally this header should not be needed, but we use it
// to dynamic_cast() our renderer to see if it's the VoxelRenderer for feature detection.
// TODO: add feature detection API to Renderer interface?
#include "graphics/voxel_renderer/VoxelRenderer.hh"

namespace sp {

    GLModel::GLModel(Model *model, Renderer *renderer) : NativeModel(model, renderer) {
        static BasicMaterial defaultMat;

        for (auto primitive : model->primitives) {
            GLModel::Primitive glPrimitive;
            glPrimitive.parent = primitive;
            glPrimitive.indexBufferHandle = LoadBuffer(primitive->indexBuffer.bufferIndex);

            glPrimitive.baseColorTex = LoadTexture(primitive->materialIndex, BaseColor);
            glPrimitive.metallicRoughnessTex = LoadTexture(primitive->materialIndex, MetallicRoughness);
            glPrimitive.heightTex = LoadTexture(primitive->materialIndex, Height);

            if (!glPrimitive.baseColorTex) glPrimitive.baseColorTex = &defaultMat.baseColorTex;
            if (!glPrimitive.metallicRoughnessTex) glPrimitive.metallicRoughnessTex = &defaultMat.metallicRoughnessTex;
            if (!glPrimitive.heightTex) glPrimitive.heightTex = &defaultMat.heightTex;

            glCreateVertexArrays(1, &glPrimitive.vertexBufferHandle);
            for (int i = 0; i < std::size(primitive->attributes); i++) {
                auto *attr = &primitive->attributes[i];
                if (attr->componentCount == 0) continue;
                glEnableVertexArrayAttrib(glPrimitive.vertexBufferHandle, i);

                if (attr->componentType == GL_UNSIGNED_SHORT) {
                    glVertexArrayAttribIFormat(glPrimitive.vertexBufferHandle,
                                               i,
                                               attr->componentCount,
                                               attr->componentType,
                                               0);
                } else {
                    glVertexArrayAttribFormat(glPrimitive.vertexBufferHandle,
                                              i,
                                              attr->componentCount,
                                              attr->componentType,
                                              GL_FALSE,
                                              0);
                }

                glVertexArrayVertexBuffer(glPrimitive.vertexBufferHandle,
                                          i,
                                          LoadBuffer(attr->bufferIndex),
                                          attr->byteOffset,
                                          attr->byteStride);
            }

            AddPrimitive(glPrimitive);
        }
    }

    GLModel::~GLModel() {
        for (auto primitive : primitives) {
            glDeleteVertexArrays(1, &primitive.vertexBufferHandle);
        }
        for (auto buf : buffers) {
            glDeleteBuffers(1, &buf.second);
        }
        for (auto tex : textures) {
            tex.second.Delete();
        }
    }

    void GLModel::AddPrimitive(GLModel::Primitive prim) {
        primitives.push_back(prim);
    }

    void GLModel::Draw(SceneShader *shader,
                       glm::mat4 modelMat,
                       const ecs::View &view,
                       int boneCount,
                       glm::mat4 *boneData) {
        for (auto primitive : primitives) {
            glBindVertexArray(primitive.vertexBufferHandle);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primitive.indexBufferHandle);

            if (primitive.baseColorTex) primitive.baseColorTex->Bind(0);

            if (primitive.metallicRoughnessTex) primitive.metallicRoughnessTex->Bind(1);

            if (primitive.heightTex) primitive.heightTex->Bind(3);

            shader->SetParams(view, modelMat, primitive.parent->matrix);

            if (boneCount > 0) {
                // TODO: upload vec3 and quat instead of a mat4 to save memory bw
                shader->SetBoneData(boneCount, boneData);
            }

            glDrawElements(primitive.parent->drawMode,
                           primitive.parent->indexBuffer.components,
                           primitive.parent->indexBuffer.componentType,
                           (char *)primitive.parent->indexBuffer.byteOffset);
        }
    }

    GLuint GLModel::LoadBuffer(int index) {
        if (buffers.count(index)) return buffers[index];

        auto buffer = model->GetModel()->buffers[index];
        GLuint handle;
        glCreateBuffers(1, &handle);
        glNamedBufferData(handle, buffer.data.size(), buffer.data.data(), GL_STATIC_DRAW);
        buffers[index] = handle;
        return handle;
    }

    GLTexture *GLModel::LoadTexture(int materialIndex, TextureType textureType) {
        auto &material = model->GetModel()->materials[materialIndex];

        string name = std::to_string(materialIndex) + "_";
        int textureIndex = -1;
        std::vector<double> factor;

        switch (textureType) {
        case BaseColor:
            name += std::to_string(material.pbrMetallicRoughness.baseColorTexture.index) + "_BASE";
            textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
            factor = material.pbrMetallicRoughness.baseColorFactor;
            break;

        // gltf2.0 uses a combined texture for metallic roughness.
        // Roughness = G channel, Metallic = B channel.
        // R and A channels are not used / should be ignored.
        case MetallicRoughness:
            name += std::to_string(material.pbrMetallicRoughness.metallicRoughnessTexture.index) + "_METALICROUGHNESS";
            textureIndex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
            factor = {0.0,
                      material.pbrMetallicRoughness.roughnessFactor,
                      material.pbrMetallicRoughness.metallicFactor,
                      0.0};
            break;

        case Height:
            name += std::to_string(material.normalTexture.index) + "_HEIGHT";
            textureIndex = material.normalTexture.index;
            // factor not supported for height textures
            break;

        case Occlusion:
            name += std::to_string(material.occlusionTexture.index) + "_OCCLUSION";
            textureIndex = material.occlusionTexture.index;
            // factor not supported for occlusion textures
            break;

        case Emissive:
            name += std::to_string(material.occlusionTexture.index) + "_EMISSIVE";
            textureIndex = material.emissiveTexture.index;
            factor = material.emissiveFactor;
            break;

        default:
            return NULL;
        }

        // Test if we have already cached this texture
        if (textures.count(name)) return &textures[name];

        // Need to create a texture for this Material / Type combo
        if (textureIndex != -1) {
            tinygltf::Texture texture = model->GetModel()->textures[textureIndex];
            tinygltf::Image img = model->GetModel()->images[texture.source];

            GLenum minFilter = GL_LINEAR_MIPMAP_LINEAR;
            GLenum magFilter = GL_LINEAR;
            GLenum wrapS = GL_REPEAT;
            GLenum wrapT = GL_REPEAT;

            if (texture.sampler != -1) {
                tinygltf::Sampler sampler = model->GetModel()->samplers[texture.sampler];

                minFilter = sampler.minFilter > 0 ? sampler.minFilter : GL_LINEAR_MIPMAP_LINEAR;
                magFilter = sampler.magFilter > 0 ? sampler.magFilter : GL_LINEAR;

                wrapS = sampler.wrapS;
                wrapT = sampler.wrapT;
            }

            GLenum format = GL_NONE;
            if (img.component == 4) {
                format = GL_RGBA;
            } else if (img.component == 3) {
                format = GL_RGB;
            } else if (img.component == 2) {
                format = GL_RG;
            } else if (img.component == 1) {
                format = GL_RED;
            } else {
                Errorf("Failed to load image at index %d: invalid number of image components (%d)",
                       texture.source,
                       img.component);
            }

            GLenum type = GL_NONE;
            if (img.bits == 8) {
                type = GL_UNSIGNED_BYTE;
            } else if (img.bits == 16) {
                type = GL_UNSIGNED_SHORT;
            } else {
                Errorf("Failed to load image at index %d: invalid pixel bit width (%d)", texture.source, img.bits);
            }

            // if (factor.size() > 0)
            // {
            // 	if (type == GL_UNSIGNED_BYTE)
            // 	{
            // 		for(size_t i = 0; i < img.image.size(); i += img.component)
            // 		{
            // 			for(size_t j = 0; j < img.component; j++)
            // 			{
            // 				img.image.data()[i + j] *= factor.at(std::min(factor.size() - 1, j));
            // 			}
            // 		}
            // 	}
            // 	else
            // 	{
            // 		throw std::runtime_error("Scaling textures that are not GL_UNSIGNED_BYTE is not supported");
            // 	}
            // }

            GLTexture *tex =
                &textures[name]
                     .Create()
                     .Filter(minFilter, magFilter, 4.0)
                     .Wrap(wrapS, wrapT)
                     .Size(img.width, img.height)
                     .Storage(GL_RGBA, format, type, GLTexture::FullyMipmap, false) // textureType == BaseColor)
                     .Image2D(img.image.data(), 0, 0, 0, 0, 0, false);

            if (VoxelRenderer *r = dynamic_cast<VoxelRenderer *>(renderer)) {
                r->GlobalShaders->Get<TextureFactorCS>()->SetFactor(std::min(img.component, (int)factor.size()),
                                                                    factor.data());
                tex->BindImageConvert(0, GLPixelFormat::PixelFormatMapping(PF_RGBA8), GL_READ_WRITE);

                r->ShaderControl->BindPipeline<TextureFactorCS>();
                // Divide image size by 16 work grounds, rounding up.
                glDispatchCompute((img.width + 15) / 16, (img.height + 15) / 16, 1);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            } else {
                throw std::runtime_error("Scaling textures is not supported");
            }

            tex->GenMipmap();

            return tex;
        } else if (factor.size() > 0) {
            unsigned char data[4];
            for (size_t i = 0; i < 4; i++) {
                data[i] = 255 * factor.at(std::min(factor.size() - 1, i));
            }

            // Create a single pixel texture based on the factor data provided
            return &textures[name]
                        .Create()
                        .Filter(GL_NEAREST, GL_NEAREST)
                        .Wrap(GL_REPEAT, GL_REPEAT)
                        .Size(1, 1)
                        .Storage(PF_RGBA8)
                        .Image2D(data);
        }

        return NULL;
    }

}; // namespace sp