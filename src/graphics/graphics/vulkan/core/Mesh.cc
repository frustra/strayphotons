#include "Mesh.hh"

#include "assets/Gltf.hh"
#include "assets/GltfImpl.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/vulkan/VertexLayouts.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

#include <tiny_gltf.h>

namespace sp::vulkan {
    Mesh::Mesh(std::shared_ptr<const sp::Gltf> source, size_t meshIndex, GPUSceneContext &scene, DeviceContext &device)
        : modelName(source->name), scene(scene), asset(source), meshIndex(meshIndex) {
        ZoneScoped;
        ZonePrintf("%s.%u", modelName, meshIndex);
        // TODO: cache the output somewhere. Keeping the conversion code in
        // the engine will be useful for any dynamic loading in the future,
        // but we don't want to do it every time a model is loaded.

        Assertf(meshIndex < source->meshes.size(), "Mesh index is out of range: %s.%u", modelName, meshIndex);
        auto &mesh = source->meshes[meshIndex];
        Assertf(mesh, "Mesh is undefined: %s.%u", modelName, meshIndex);
        for (auto &assetPrimitive : mesh->primitives) {
            indexCount += assetPrimitive.indexBuffer.Count();
            vertexCount += assetPrimitive.positionBuffer.Count();
        }

        indexBuffer = scene.indexBuffer->ArrayAllocate(indexCount, sizeof(uint32));
        auto indexData = (uint32 *)indexBuffer->Mapped();
        auto indexDataStart = indexData;

        vertexBuffer = scene.vertexBuffer->ArrayAllocate(vertexCount, sizeof(SceneVertex));
        auto vertexData = (SceneVertex *)vertexBuffer->Mapped();
        auto vertexDataStart = vertexData;

        for (auto &assetPrimitive : mesh->primitives) {
            ZoneScopedN("CreatePrimitive");
            // TODO: this implementation assumes a lot about the model format,
            // and asserts the assumptions. It would be better to support more
            // kinds of inputs, and convert the data rather than just failing.
            Assert(assetPrimitive.drawMode == sp::gltf::Mesh::DrawMode::Triangles, "draw mode must be Triangles");

            auto &vkPrimitive = primitives.emplace_back();

            vkPrimitive.indexCount = assetPrimitive.indexBuffer.Count();
            vkPrimitive.indexOffset = indexData - indexDataStart;

            for (size_t i = 0; i < vkPrimitive.indexCount; i++) {
                *indexData++ = assetPrimitive.indexBuffer.Read(i);
            }

            vkPrimitive.vertexCount = assetPrimitive.positionBuffer.Count();
            vkPrimitive.vertexOffset = vertexData - vertexDataStart;

            for (size_t i = 0; i < vkPrimitive.vertexCount; i++) {
                SceneVertex &vertex = *vertexData++;

                vertex.position = assetPrimitive.positionBuffer.Read(i);
                if (i < assetPrimitive.normalBuffer.Count()) vertex.normal = assetPrimitive.normalBuffer.Read(i);
                if (i < assetPrimitive.texcoordBuffer.Count()) vertex.uv = assetPrimitive.texcoordBuffer.Read(i);
            }

            vkPrimitive.baseColor = LoadTexture(device, source, assetPrimitive.materialIndex, TextureType::BaseColor);
            vkPrimitive.metallicRoughness =
                LoadTexture(device, source, assetPrimitive.materialIndex, TextureType::MetallicRoughness);
        }

        primitiveList = scene.primitiveLists->ArrayAllocate(primitives.size(), sizeof(GPUMeshPrimitive));
        modelEntry = scene.models->ArrayAllocate(1, sizeof(GPUMeshModel));

        auto meshModel = (GPUMeshModel *)modelEntry->Mapped();
        auto gpuPrimitives = (GPUMeshPrimitive *)primitiveList->Mapped();
        {
            ZoneScopedN("CopyPrimitives");
            auto gpuPrim = gpuPrimitives;
            for (auto &p : primitives) {
                gpuPrim->indexCount = p.indexCount;
                gpuPrim->vertexCount = p.vertexCount;
                gpuPrim->firstIndex = p.indexOffset;
                gpuPrim->vertexOffset = p.vertexOffset;
                gpuPrim->baseColorTexID = p.baseColor;
                gpuPrim->metallicRoughnessTexID = p.metallicRoughness;
                gpuPrim++;
            }
            meshModel->primitiveCount = primitives.size();
            meshModel->primitiveOffset = primitiveList->ArrayOffset();
            meshModel->indexOffset = indexBuffer->ArrayOffset();
            meshModel->vertexOffset = vertexBuffer->ArrayOffset();
        }
    }

    Mesh::~Mesh() {
        ZoneScoped;
        ZoneStr(modelName);

        for (auto it : textures) {
            scene.ReleaseTexture(it.second);
        }
    }

    uint32 Mesh::SceneIndex() const {
        return modelEntry->ArrayOffset();
    }

    TextureIndex Mesh::LoadTexture(DeviceContext &device,
        const shared_ptr<const sp::Gltf> &source,
        int materialIndex,
        TextureType type) {
        ZoneScoped;
        if (!source) {
            Errorf("Mesh::LoadTexture called with null source");
            return 0;
        }
        auto &gltfModel = *source->gltfModel;
        if (materialIndex < 0 || (size_t)materialIndex >= gltfModel.materials.size()) {
            Errorf("Mesh::LoadTexture called with invalid materialIndex: %d", materialIndex);
            return 0;
        }
        auto &material = gltfModel.materials[materialIndex];

        string name = std::to_string(materialIndex) + "_";
        int textureIndex = -1;
        std::vector<double> factor;
        bool srgb = false;

        switch (type) {
        case TextureType::BaseColor:
            name += std::to_string(material.pbrMetallicRoughness.baseColorTexture.index) + "_BASE";
            textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
            factor = material.pbrMetallicRoughness.baseColorFactor;
            srgb = true;
            break;

        // gltf2.0 uses a combined texture for metallic roughness.
        // Roughness = G channel, Metallic = B channel.
        // R and A channels are not used / should be ignored.
        // https://github.com/KhronosGroup/glTF/blob/e5519ce050/specification/2.0/schema/material.pbrMetallicRoughness.schema.json
        case TextureType::MetallicRoughness: {
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
        case TextureType::Height:
            name += std::to_string(material.normalTexture.index) + "_HEIGHT";
            textureIndex = material.normalTexture.index;
            // factor not supported for height textures
            break;

        case TextureType::Occlusion:
            name += std::to_string(material.occlusionTexture.index) + "_OCCLUSION";
            textureIndex = material.occlusionTexture.index;
            // factor not supported for occlusion textures
            break;

        case TextureType::Emissive:
            name += std::to_string(material.occlusionTexture.index) + "_EMISSIVE";
            textureIndex = material.emissiveTexture.index;
            factor = material.emissiveFactor;
            break;

        default:
            return 0;
        }

        if (textures.count(name)) return textures[name];

        if (textureIndex < 0 || (size_t)textureIndex >= gltfModel.textures.size()) {
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

        auto &texture = gltfModel.textures[textureIndex];
        if (texture.source < 0 || (size_t)texture.source >= gltfModel.images.size()) {
            Errorf("Gltf texture %d has invalid texture source: %d", textureIndex, texture.source);
            return 0;
        }
        auto &img = gltfModel.images[texture.source];

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
        if (texture.sampler < 0 || (size_t)texture.sampler >= gltfModel.samplers.size()) {
            viewInfo.defaultSampler = device.GetSampler(SamplerType::TrilinearTiled);
            imageInfo.genMipmap = true;
        } else {
            auto &sampler = gltfModel.samplers[texture.sampler];
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

        auto added = scene.AddTexture(imageInfo, viewInfo, {img.image.data(), img.image.size(), source});
        textures[name] = added.first;
        pendingWork.push_back(added.second);
        return added.first;
    }
} // namespace sp::vulkan
