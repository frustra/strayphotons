#include "Model.hh"

#include "CommandContext.hh"
#include "DeviceContext.hh"
#include "Renderer.hh"
#include "Vertex.hh"
#include "assets/Model.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <tiny_gltf.h>

namespace sp::vulkan {
    Model::Model(const sp::Model &model, DeviceContext &device) : modelName(model.name) {
        vector<SceneVertex> vertices;

        // TODO: cache the output somewhere. Keeping the conversion code in
        // the engine will be useful for any dynamic loading in the future,
        // but we don't want to do it every time a model is loaded.
        for (auto &assetPrimitive : model.Primitives()) {
            // TODO: this implementation assumes a lot about the model format,
            // and asserts the assumptions. It would be better to support more
            // kinds of inputs, and convert the data rather than just failing.
            Assert(assetPrimitive.drawMode == sp::Model::DrawMode::Triangles, "draw mode must be Triangles");

            auto vkPrimitivePtr = make_shared<Primitive>();
            auto &vkPrimitive = *vkPrimitivePtr;

            vkPrimitive.transform = assetPrimitive.matrix;
            auto &buffers = model.GetGltfModel()->buffers;

            switch (assetPrimitive.indexBuffer.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                vkPrimitive.indexType = vk::IndexType::eUint32;
                Assert(assetPrimitive.indexBuffer.byteStride == 4, "index buffer must be tightly packed");
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                vkPrimitive.indexType = vk::IndexType::eUint16;
                Assert(assetPrimitive.indexBuffer.byteStride == 2, "index buffer must be tightly packed");
                break;
            }
            Assert(vkPrimitive.indexType != vk::IndexType::eNoneKHR, "unimplemented vertex index type");

            auto &indexBuffer = buffers[assetPrimitive.indexBuffer.bufferIndex];
            size_t indexBufferSize = assetPrimitive.indexBuffer.componentCount * assetPrimitive.indexBuffer.byteStride;
            Assert(assetPrimitive.indexBuffer.byteOffset + indexBufferSize <= indexBuffer.data.size(),
                   "indexes overflow buffer");

            vkPrimitive.indexBuffer = device.AllocateBuffer(indexBufferSize,
                                                            vk::BufferUsageFlagBits::eIndexBuffer,
                                                            VMA_MEMORY_USAGE_CPU_TO_GPU);

            void *data;
            vkPrimitive.indexBuffer->Map(&data);
            memcpy(data, &indexBuffer.data[assetPrimitive.indexBuffer.byteOffset], indexBufferSize);
            vkPrimitive.indexBuffer->Unmap();

            vkPrimitive.indexCount = assetPrimitive.indexBuffer.componentCount;

            auto &posAttr = assetPrimitive.attributes[0];
            auto &normalAttr = assetPrimitive.attributes[1];
            auto &uvAttr = assetPrimitive.attributes[2];

            if (posAttr.componentCount) {
                Assert(posAttr.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT,
                       "position attribute must be a float vector");
                Assert(posAttr.componentFields == 3, "position attribute must be a vec3");
            }
            if (normalAttr.componentCount) {
                Assert(normalAttr.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT,
                       "normal attribute must be a float vector");
                Assert(normalAttr.componentFields == 3, "normal attribute must be a vec3");
            }
            if (uvAttr.componentCount) {
                Assert(uvAttr.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT, "uv attribute must be a float vector");
                Assert(uvAttr.componentFields == 2, "uv attribute must be a vec2");
            }

            vertices.resize(posAttr.componentCount);

            for (size_t i = 0; i < posAttr.componentCount; i++) {
                SceneVertex &vertex = vertices[i];

                vertex.position = reinterpret_cast<const glm::vec3 &>(
                    buffers[posAttr.bufferIndex].data[posAttr.byteOffset + i * posAttr.byteStride]);

                if (normalAttr.componentCount) {
                    vertex.normal = reinterpret_cast<const glm::vec3 &>(
                        buffers[normalAttr.bufferIndex].data[normalAttr.byteOffset + i * normalAttr.byteStride]);
                }

                if (uvAttr.componentCount) {
                    vertex.uv = reinterpret_cast<const glm::vec2 &>(
                        buffers[uvAttr.bufferIndex].data[uvAttr.byteOffset + i * uvAttr.byteStride]);
                }
            }

            size_t vertexBufferSize = vertices.size() * sizeof(vertices[0]);
            vkPrimitive.vertexBuffer = device.AllocateBuffer(vertexBufferSize,
                                                             vk::BufferUsageFlagBits::eVertexBuffer,
                                                             VMA_MEMORY_USAGE_CPU_TO_GPU);

            vkPrimitive.vertexBuffer->Map(&data);
            memcpy(data, vertices.data(), vertexBufferSize);
            vkPrimitive.vertexBuffer->Unmap();

            vkPrimitive.baseColor = LoadTexture(device, model, assetPrimitive.materialIndex, BaseColor);
            vkPrimitive.metallicRoughness = LoadTexture(device, model, assetPrimitive.materialIndex, MetallicRoughness);
            // TODO: add default textures

            primitives.emplace_back(vkPrimitivePtr);
        }
    }

    Model::~Model() {
        Debugf("Destroying vulkan::Model %s", modelName);
    }

    void Model::Draw(const CommandContextPtr &cmd, glm::mat4 modelMat) {
        cmd->SetVertexLayout(SceneVertex::Layout());

        for (auto &primitivePtr : primitives) {
            auto &primitive = *primitivePtr;
            MeshPushConstants constants;
            constants.model = modelMat * primitive.transform;

            cmd->PushConstants(&constants, 0, sizeof(MeshPushConstants));

            if (primitive.baseColor) cmd->SetTexture(0, 0, primitive.baseColor);
            if (primitive.metallicRoughness) cmd->SetTexture(0, 1, primitive.metallicRoughness);

            cmd->Raw().bindIndexBuffer(*primitive.indexBuffer, 0, primitive.indexType);
            cmd->Raw().bindVertexBuffers(0, {*primitive.vertexBuffer}, {0});
            cmd->DrawIndexed(primitive.indexCount, 1, 0, 0, 0);
        }
    }

    ImageViewPtr Model::LoadTexture(DeviceContext &device,
                                    const sp::Model &model,
                                    int materialIndex,
                                    TextureType type) {
        auto &gltfModel = model.GetGltfModel();
        auto &material = gltfModel->materials[materialIndex];

        string name = std::to_string(materialIndex) + "_";
        int textureIndex = -1;
        std::vector<double> factor;

        switch (type) {
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

        if (textures.count(name)) return textures[name];

        if (textureIndex == -1) {
            if (factor.size() == 0) return nullptr;

            uint8_t data[4];
            for (size_t i = 0; i < 4; i++) {
                data[i] = (uint8_t)(255.0 * factor.at(std::min(factor.size() - 1, i)));
            }

            // Create a single pixel texture based on the factor data provided
            vk::ImageCreateInfo imageInfo;
            imageInfo.imageType = vk::ImageType::e2D;
            imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
            imageInfo.format = vk::Format::eR8G8B8A8Unorm;
            imageInfo.extent = vk::Extent3D(1, 1, 1);

            ImageViewCreateInfo viewInfo;
            viewInfo.defaultSampler = device.GetSampler(SamplerType::NearestTiled);
            auto imageView = device.CreateImageAndView(imageInfo, viewInfo, data, sizeof(data));
            textures[name] = imageView;
            return imageView;
        }

        tinygltf::Texture texture = gltfModel->textures[textureIndex];
        tinygltf::Image img = gltfModel->images[texture.source];

        vk::ImageCreateInfo imageInfo;
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
        imageInfo.format = FormatFromTraits(img.component, img.bits, true);

        if (imageInfo.format == vk::Format::eUndefined) {
            Errorf("Failed to load image at index %d: invalid format with components=%d and bits=%d",
                   texture.source,
                   img.component,
                   img.bits);
            return nullptr;
        }

        imageInfo.extent = vk::Extent3D(img.width, img.height, 1);

        ImageViewCreateInfo viewInfo;
        if (texture.sampler == -1) {
            viewInfo.defaultSampler = device.GetSampler(SamplerType::TrilinearTiled);
        } else {
            auto &sampler = gltfModel->samplers[texture.sampler];
            int minFilter = sampler.minFilter > 0 ? sampler.minFilter : TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
            int magFilter = sampler.magFilter > 0 ? sampler.magFilter : TINYGLTF_TEXTURE_FILTER_LINEAR;

            auto samplerInfo = GLSamplerToVKSampler(minFilter, magFilter, sampler.wrapS, sampler.wrapT, sampler.wrapR);
            if (samplerInfo.mipmapMode == vk::SamplerMipmapMode::eLinear) {
                samplerInfo.anisotropyEnable = true;
                samplerInfo.maxAnisotropy = 8.0f;
            }
            viewInfo.defaultSampler = device.GetSampler(samplerInfo);
        }

        auto imageView = device.CreateImageAndView(imageInfo, viewInfo, img.image.data(), img.image.size(), true);

        textures[name] = imageView;
        return imageView;
    }
} // namespace sp::vulkan
