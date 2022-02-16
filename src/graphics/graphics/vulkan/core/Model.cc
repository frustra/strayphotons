#include "Model.hh"

#include "assets/Model.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Vertex.hh"

#include <tiny_gltf.h>

namespace sp::vulkan {
    Model::Model(shared_ptr<const sp::Model> model, GPUSceneContext &scene, DeviceContext &device)
        : modelName(model->name), scene(scene), asset(model) {
        ZoneScoped;
        ZoneStr(modelName);
        // TODO: cache the output somewhere. Keeping the conversion code in
        // the engine will be useful for any dynamic loading in the future,
        // but we don't want to do it every time a model is loaded.

        for (auto &assetPrimitive : model->Primitives()) {
            indexCount += assetPrimitive.indexBuffer.componentCount;
            vertexCount += assetPrimitive.attributes[0].componentCount;
        }

        indexBuffer = scene.indexBuffer->ArrayAllocate(indexCount, sizeof(uint16));
        auto indexData = (uint16 *)indexBuffer->Mapped();
        auto indexDataStart = indexData;

        vertexBuffer = scene.vertexBuffer->ArrayAllocate(vertexCount, sizeof(SceneVertex));
        auto vertexData = (SceneVertex *)vertexBuffer->Mapped();
        auto vertexDataStart = vertexData;

        for (auto &assetPrimitive : model->Primitives()) {
            // TODO: this implementation assumes a lot about the model format,
            // and asserts the assumptions. It would be better to support more
            // kinds of inputs, and convert the data rather than just failing.
            Assert(assetPrimitive.drawMode == sp::Model::DrawMode::Triangles, "draw mode must be Triangles");

            auto vkPrimitivePtr = make_shared<Primitive>();
            auto &vkPrimitive = *vkPrimitivePtr;

            vkPrimitive.transform = assetPrimitive.matrix;
            auto &buffers = model->GetGltfModel()->buffers;

            switch (assetPrimitive.indexBuffer.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                vkPrimitive.indexType = vk::IndexType::eUint32;
                Assert(assetPrimitive.indexBuffer.byteStride == 4, "index buffer must be tightly packed");
                Abortf("TODO %s uses uint indexes, but the GPU driven renderer doesn't support them yet", modelName);
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                vkPrimitive.indexType = vk::IndexType::eUint16;
                Assert(assetPrimitive.indexBuffer.byteStride == 2, "index buffer must be tightly packed");
                break;
            }
            Assert(vkPrimitive.indexType != vk::IndexType::eNoneKHR, "unimplemented vertex index type");

            auto &indexBufferSrc = buffers[assetPrimitive.indexBuffer.bufferIndex];
            vkPrimitive.indexCount = assetPrimitive.indexBuffer.componentCount;
            vkPrimitive.indexOffset = indexData - indexDataStart;
            size_t indexBufferSize = vkPrimitive.indexCount * assetPrimitive.indexBuffer.byteStride;
            Assert(assetPrimitive.indexBuffer.byteOffset + indexBufferSize <= indexBufferSrc.data.size(),
                "indexes overflow buffer");

            auto indexPtr = &indexBufferSrc.data[assetPrimitive.indexBuffer.byteOffset];
            {
                ZoneScopedN("CopyIndexData");
                std::copy(indexPtr, indexPtr + indexBufferSize, (uint8 *)indexData);
            }
            indexData += vkPrimitive.indexCount;

            auto posAttr = assetPrimitive.attributes[0];
            auto normalAttr = assetPrimitive.attributes[1];
            auto uvAttr = assetPrimitive.attributes[2];

            size_t vertexCount = posAttr.componentCount;

            if (vertexCount) {
                Assert(posAttr.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT,
                    "position attribute must be a float vector");
                Assert(posAttr.componentFields == 3, "position attribute must be a vec3");
            }
            if (normalAttr.componentCount) {
                Assert(normalAttr.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT,
                    "normal attribute must be a float vector");
                Assert(normalAttr.componentFields == 3, "normal attribute must be a vec3");
                Assert(normalAttr.componentCount == vertexCount, "must have a normal for every vertex");
            }
            if (uvAttr.componentCount) {
                Assert(uvAttr.componentType == TINYGLTF_PARAMETER_TYPE_FLOAT, "uv attribute must be a float vector");
                Assert(uvAttr.componentFields == 2, "uv attribute must be a vec2");
                Assert(uvAttr.componentCount == vertexCount, "must have texcoords for every vertex");
            }

            vkPrimitive.vertexCount = posAttr.componentCount;
            vkPrimitive.vertexOffset = vertexData - vertexDataStart;

            const uint8 *posBuffer = buffers[posAttr.bufferIndex].data.data() + posAttr.byteOffset;
            const uint8 *normalBuffer = buffers[normalAttr.bufferIndex].data.data() + normalAttr.byteOffset;
            const uint8 *uvBuffer = buffers[uvAttr.bufferIndex].data.data() + uvAttr.byteOffset;

            {
                ZoneScopedN("CopyVertexData");
                for (size_t i = 0; i < vertexCount; i++) {
                    SceneVertex &vertex = vertexData[i];

                    vertex.position = reinterpret_cast<const glm::vec3 &>(posBuffer[i * posAttr.byteStride]);

                    if (i < normalAttr.componentCount) {
                        vertex.normal = reinterpret_cast<const glm::vec3 &>(normalBuffer[i * normalAttr.byteStride]);
                    }

                    if (i < uvAttr.componentCount) {
                        vertex.uv = reinterpret_cast<const glm::vec2 &>(uvBuffer[i * uvAttr.byteStride]);
                    }
                }
            }

            vertexData += vertexCount;

            vkPrimitive.baseColor = LoadTexture(device, *model, assetPrimitive.materialIndex, BaseColor);
            vkPrimitive.metallicRoughness =
                LoadTexture(device, *model, assetPrimitive.materialIndex, MetallicRoughness);

            primitives.emplace_back(vkPrimitivePtr);
        }

        primitiveList = scene.primitiveLists->ArrayAllocate(primitives.size(), sizeof(GPUMeshPrimitive));
        modelEntry = scene.models->ArrayAllocate(1, sizeof(GPUMeshModel));

        auto meshModel = (GPUMeshModel *)modelEntry->Mapped();
        auto gpuPrimitives = (GPUMeshPrimitive *)primitiveList->Mapped();
        {
            ZoneScopedN("CopyPrimitives");
            auto gpuPrim = gpuPrimitives;
            for (auto &p : primitives) {
                gpuPrim->indexCount = p->indexCount;
                gpuPrim->vertexCount = p->vertexCount;
                gpuPrim->firstIndex = p->indexOffset;
                gpuPrim->vertexOffset = p->vertexOffset;
                gpuPrim->primitiveToModel = p->transform;
                gpuPrim->baseColorTexID = p->baseColor;
                gpuPrim->metallicRoughnessTexID = p->metallicRoughness;
                gpuPrim++;
            }
            meshModel->primitiveCount = primitives.size();
            meshModel->primitiveOffset = primitiveList->ArrayOffset();
            meshModel->indexOffset = indexBuffer->ArrayOffset();
            meshModel->vertexOffset = vertexBuffer->ArrayOffset();
        }
    }

    Model::~Model() {
        ZoneScoped;
        ZoneStr(modelName);

        for (auto it : textures) {
            scene.ReleaseTexture(it.second);
        }
    }

    uint32 Model::SceneIndex() const {
        return modelEntry->ArrayOffset();
    }

    void Model::Draw(CommandContext &cmd, glm::mat4 modelMat, bool useMaterial) {
        cmd.SetVertexLayout(SceneVertex::Layout());

        for (auto &primitivePtr : primitives) {
            auto &primitive = *primitivePtr;
            MeshPushConstants constants;
            constants.model = modelMat * primitive.transform;

            cmd.PushConstants(constants);

            if (useMaterial) {
                cmd.SetTexture(0, 0, scene.GetTexture(primitive.baseColor));
                cmd.SetTexture(0, 1, scene.GetTexture(primitive.metallicRoughness));
            }

            cmd.Raw().bindIndexBuffer(*scene.indexBuffer,
                indexBuffer->ByteOffset() + primitive.indexOffset * sizeof(uint16),
                primitive.indexType);
            cmd.Raw().bindVertexBuffers(0,
                {*scene.vertexBuffer},
                {vertexBuffer->ByteOffset() + primitive.vertexOffset * sizeof(SceneVertex)});
            cmd.DrawIndexed(primitive.indexCount, 1, 0, 0, 0);
        }
    }

    TextureIndex Model::LoadTexture(DeviceContext &device,
        const sp::Model &model,
        int materialIndex,
        TextureType type) {
        ZoneScoped;
        auto &gltfModel = model.GetGltfModel();
        auto &material = gltfModel->materials[materialIndex];

        string name = std::to_string(materialIndex) + "_";
        int textureIndex = -1;
        std::vector<double> factor;
        bool srgb = false;

        switch (type) {
        case BaseColor:
            name += std::to_string(material.pbrMetallicRoughness.baseColorTexture.index) + "_BASE";
            textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
            factor = material.pbrMetallicRoughness.baseColorFactor;
            srgb = true;
            break;

        // gltf2.0 uses a combined texture for metallic roughness.
        // Roughness = G channel, Metallic = B channel.
        // R and A channels are not used / should be ignored.
        // https://github.com/KhronosGroup/glTF/blob/e5519ce050/specification/2.0/schema/material.pbrMetallicRoughness.schema.json
        case MetallicRoughness: {
            name += std::to_string(material.pbrMetallicRoughness.metallicRoughnessTexture.index) + "_METALICROUGHNESS";
            textureIndex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
            double rf = material.pbrMetallicRoughness.roughnessFactor,
                   mf = material.pbrMetallicRoughness.metallicFactor;
            if (rf != 1 || mf != 1) factor = {0.0, rf, mf, 0.0};
            // The spec says these should be linear, but we have srgb files right now.
            // This makes sense, since there's no reason to have more precision for lower values.
            // TODO: reencode as linear
            srgb = true;
            break;
        }
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
            return 0;
        }

        if (textures.count(name)) return textures[name];

        if (textureIndex == -1) {
            if (factor.size() == 0) factor.push_back(1); // default texture is a single white pixel

            auto data = make_shared<std::array<uint8, 4>>();
            for (size_t i = 0; i < 4; i++) {
                (*data)[i] = uint8(255.0 * factor.at(std::min(factor.size() - 1, i)));
            }

            // Create a single pixel texture based on the factor data provided
            ImageCreateInfo imageInfo;
            imageInfo.imageType = vk::ImageType::e2D;
            imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
            imageInfo.format = vk::Format::eR8G8B8A8Unorm;
            imageInfo.extent = vk::Extent3D(1, 1, 1);

            ImageViewCreateInfo viewInfo;
            viewInfo.defaultSampler = device.GetSampler(SamplerType::NearestTiled);
            auto added = scene.AddTexture(imageInfo, viewInfo, {data->data(), data->size(), data});
            textures[name] = added.first;
            pendingWork.push_back(added.second);
            return added.first;
        }

        auto &texture = gltfModel->textures[textureIndex];
        auto &img = gltfModel->images[texture.source];

        ImageCreateInfo imageInfo;
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
        imageInfo.format = FormatFromTraits(img.component, img.bits, srgb);

        bool useFactor = false;
        for (auto f : factor) {
            if (f != 1) useFactor = true;
        }
        if (useFactor) imageInfo.factor = std::move(factor);

        if (imageInfo.format == vk::Format::eUndefined) {
            Errorf("Failed to load image at index %d: invalid format with components=%d and bits=%d",
                texture.source,
                img.component,
                img.bits);
            return 0;
        }

        imageInfo.extent = vk::Extent3D(img.width, img.height, 1);

        ImageViewCreateInfo viewInfo;
        if (texture.sampler == -1) {
            viewInfo.defaultSampler = device.GetSampler(SamplerType::TrilinearTiled);
            imageInfo.genMipmap = true;
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
            imageInfo.genMipmap = (samplerInfo.maxLod > 0);
        }

        auto added = scene.AddTexture(imageInfo, viewInfo, {img.image.data(), img.image.size(), gltfModel});
        textures[name] = added.first;
        pendingWork.push_back(added.second);
        return added.first;
    }
} // namespace sp::vulkan
