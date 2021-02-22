#include "assets/Model.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "core/Logging.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/voxel_renderer/VoxelRenderer.hh"

#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <murmurhash/MurmurHash3.h>

namespace sp {
    glm::mat4 GetNodeMatrix(tinygltf::Node *node) {
        glm::mat4 out(1.0);

        if (node->matrix.size() == 16) {
            std::copy(node->matrix.begin(), node->matrix.end(), glm::value_ptr(out));
        } else {
            if (node->translation.size() == 3) {
                out = glm::translate(out, glm::vec3(node->translation[0], node->translation[1], node->translation[2]));
            }

            if (node->rotation.size() == 4) {
                out = out * glm::mat4_cast(
                                glm::quat(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]));
            }

            if (node->scale.size() == 3) {
                out = glm::scale(out, glm::vec3(node->scale[0], node->scale[1], node->scale[2]));
            }
        }

        return out;
    }

    size_t byteStrideForAccessor(int componentType, size_t componentCount, size_t existingByteStride) {
        if (existingByteStride) return existingByteStride;

        size_t componentWidth = 0;

        switch (componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            componentWidth = 1;
            break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            componentWidth = 2;
            break;
        case TINYGLTF_COMPONENT_TYPE_INT:
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            componentWidth = 4;
            break;
        case TINYGLTF_COMPONENT_TYPE_DOUBLE:
            componentWidth = 8;
            break;
        default:
            Assert(false, "invalid component type");
            break;
        }

        return componentCount * componentWidth;
    }

    Model::Attribute GetPrimitiveAttribute(shared_ptr<tinygltf::Model> model,
                                           tinygltf::Primitive *p,
                                           string attribute) {
        if (!p->attributes.count(attribute)) return Model::Attribute();
        auto accessor = model->accessors[p->attributes[attribute]];
        auto bufView = model->bufferViews[accessor.bufferView];

        size_t componentCount = 1;
        if (accessor.type == TINYGLTF_TYPE_SCALAR) {
            componentCount = 1;
        } else if (accessor.type == TINYGLTF_TYPE_VEC2) {
            componentCount = 2;
        } else if (accessor.type == TINYGLTF_TYPE_VEC3) {
            componentCount = 3;
        } else if (accessor.type == TINYGLTF_TYPE_VEC4) {
            componentCount = 4;
        }

        return Model::Attribute{accessor.byteOffset + bufView.byteOffset,
                                accessor.ByteStride(bufView),
                                accessor.componentType,
                                componentCount,
                                accessor.count,
                                bufView.buffer};
    }

    Model::Model(const std::string &name, shared_ptr<tinygltf::Model> model) : name(name), model(model) {
        int defaultScene = 0;

        if (model->defaultScene != -1) { defaultScene = model->defaultScene; }

        for (int node : model->scenes[defaultScene].nodes) {
            AddNode(node, glm::mat4());
        }
    }

    Model::~Model() {
        Debugf("Destroying model %s (prepared: %d)", name, !!glModel);
        for (auto primitive : primitives) {
            delete primitive;
        }

        if (!!asset) { asset->manager->UnregisterModel(*this); }
    }

    bool Model::HasBuffer(int index) {
        return model->buffers.size() > index;
    }

    vector<unsigned char> Model::GetBuffer(int index) {
        return model->buffers[index].data;
    }

    Hash128 Model::HashBuffer(int index) {
        Hash128 output;
        auto buffer = GetBuffer(index);
        MurmurHash3_x86_128(buffer.data(), buffer.size(), 0, output.data());
        return output;
    }

    void Model::AddNode(int nodeIndex, glm::mat4 parentMatrix) {
        glm::mat4 matrix = parentMatrix * GetNodeMatrix(&model->nodes[nodeIndex]);

        // Meshes are optional on nodes
        if (model->nodes[nodeIndex].mesh != -1) {
            for (auto primitive : model->meshes[model->nodes[nodeIndex].mesh].primitives) {
                auto iAcc = model->accessors[primitive.indices];
                auto iBufView = model->bufferViews[iAcc.bufferView];

                int mode = -1;
                if (primitive.mode == TINYGLTF_MODE_TRIANGLES) {
                    mode = GL_TRIANGLES;
                } else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
                    mode = GL_TRIANGLE_STRIP;
                } else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_FAN) {
                    mode = GL_TRIANGLE_FAN;
                } else if (primitive.mode == TINYGLTF_MODE_POINTS) {
                    mode = GL_POINTS;
                } else if (primitive.mode == TINYGLTF_MODE_LINE) {
                    mode = GL_LINES;
                } else if (primitive.mode == TINYGLTF_MODE_LINE_LOOP) {
                    mode = GL_LINE_LOOP;
                };

                Assert(iAcc.type == TINYGLTF_TYPE_SCALAR, "index buffer type must be scalar");

                primitives.push_back(new Primitive{matrix,
                                                   mode,
                                                   Attribute{iAcc.byteOffset + iBufView.byteOffset,
                                                             iAcc.ByteStride(iBufView),
                                                             iAcc.componentType,
                                                             1,
                                                             iAcc.count,
                                                             iBufView.buffer},
                                                   primitive.material,
                                                   {
                                                       GetPrimitiveAttribute(model, &primitive, "POSITION"),
                                                       GetPrimitiveAttribute(model, &primitive, "NORMAL"),
                                                       GetPrimitiveAttribute(model, &primitive, "TEXCOORD_0"),
                                                       GetPrimitiveAttribute(model, &primitive, "WEIGHTS_0"),
                                                       GetPrimitiveAttribute(model, &primitive, "JOINTS_0"),
                                                   }});
            }

            // Must have a mesh to have a skin
            if (model->nodes[nodeIndex].skin != -1) {
                int skinId = model->nodes[nodeIndex].skin;
                int inverseBindMatrixAccessor = model->skins[skinId].inverseBindMatrices;

                rootBone = model->skins[skinId].skeleton;

                // Verify that the inverse bind matrix accessor has the name number of elements
                // as the skin.joints vector (if it exists)
                if (inverseBindMatrixAccessor != -1) {
                    if (model->accessors[inverseBindMatrixAccessor].count != model->skins[skinId].joints.size()) {
                        throw std::runtime_error("Invalid GLTF: mismatched inverse bind matrix and skin joints number");
                    }

                    if (model->accessors[inverseBindMatrixAccessor].type != TINYGLTF_TYPE_MAT4) {
                        throw std::runtime_error("Invalid GLTF: inverse bind matrix is not mat4");
                    }

                    if (model->accessors[inverseBindMatrixAccessor].componentType != TINYGLTF_PARAMETER_TYPE_FLOAT) {
                        throw std::runtime_error("Invalid GLTF: inverse bind matrix is not float");
                    }
                }

                for (int i = 0; i < model->skins[skinId].joints.size(); i++) {
                    if (inverseBindMatrixAccessor != -1) {
                        int bufferView = model->accessors[inverseBindMatrixAccessor].bufferView;
                        int buffer = model->bufferViews[bufferView].buffer;
                        int byteStride = model->accessors[inverseBindMatrixAccessor].ByteStride(
                            model->bufferViews[bufferView]);
                        int byteOffset = model->accessors[inverseBindMatrixAccessor].byteOffset +
                                         model->bufferViews[bufferView].byteOffset;

                        int dataOffset = byteOffset + (i * byteStride);

                        inverseBindMatrixForJoint[model->skins[skinId].joints[i]] = glm::make_mat4(
                            (float *)((uintptr_t)model->buffers[buffer].data.data() + dataOffset));
                    } else {
                        // If no inv bind matrix is supplied by the model, GLTF standard says use a 4x4 identity matrix
                        inverseBindMatrixForJoint[model->skins[skinId].joints[i]] = glm::mat4(1.0f);
                    }
                }
            }
        }

        for (int childNodeIndex : model->nodes[nodeIndex].children) {
            AddNode(childNodeIndex, matrix);
        }
    }

    // Returns a vector of the GLTF node indexes that are present in the "joints"
    // array of the GLTF skin.
    vector<int> Model::GetJointNodes() {
        vector<int> nodes;

        // TODO: deal with GLTFs that have more than one skin
        for (int node : model->skins[0].joints) {
            nodes.push_back(node);
        }

        return nodes;
    }

    int Model::FindNodeByName(std::string name) {
        for (int i = 0; i < model->nodes.size(); i++) {
            if (model->nodes[i].name == name) { return i; }
        }
        return -1;
    }

    glm::mat4 Model::GetInvBindPoseForNode(int nodeIndex) {
        return inverseBindMatrixForJoint[nodeIndex];
    }

    string Model::GetNodeName(int node) {
        return model->nodes[node].name;
    }

    GLModel::GLModel(Model *model, Renderer *renderer) : model(model), renderer(renderer) {
        static BasicMaterial defaultMat;

        for (auto primitive : model->primitives) {
            Primitive glPrimitive;
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

    void GLModel::AddPrimitive(Primitive prim) {
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

        auto buffer = model->model->buffers[index];
        GLuint handle;
        glCreateBuffers(1, &handle);
        glNamedBufferData(handle, buffer.data.size(), buffer.data.data(), GL_STATIC_DRAW);
        buffers[index] = handle;
        return handle;
    }

    Texture *GLModel::LoadTexture(int materialIndex, TextureType textureType) {
        auto &material = model->model->materials[materialIndex];

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
            tinygltf::Texture texture = model->model->textures[textureIndex];
            tinygltf::Image img = model->model->images[texture.source];

            GLenum minFilter = GL_LINEAR_MIPMAP_LINEAR;
            GLenum magFilter = GL_LINEAR;
            GLenum wrapS = GL_REPEAT;
            GLenum wrapT = GL_REPEAT;

            if (texture.sampler != -1) {
                tinygltf::Sampler sampler = model->model->samplers[texture.sampler];

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

            Texture *tex =
                &textures[name]
                     .Create()
                     .Filter(minFilter, magFilter, 4.0)
                     .Wrap(wrapS, wrapT)
                     .Size(img.width, img.height)
                     .Storage(GL_RGBA, format, type, Texture::FullyMipmap, false) // textureType == BaseColor)
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
} // namespace sp
