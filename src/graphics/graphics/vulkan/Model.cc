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
        for (auto &primitive : model.Primitives()) {
            // TODO: this implementation assumes a lot about the model format,
            // and asserts the assumptions. It would be better to support more
            // kinds of inputs, and convert the data rather than just failing.
            Assert(primitive.drawMode == sp::Model::DrawMode::Triangles, "draw mode must be Triangles");

            auto &p = *primitives.emplace_back(make_shared<Primitive>());
            p.transform = primitive.matrix;
            auto &buffers = model.GetGltfModel()->buffers;

            switch (primitive.indexBuffer.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                p.indexType = vk::IndexType::eUint32;
                Assert(primitive.indexBuffer.byteStride == 4, "index buffer must be tightly packed");
                break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                p.indexType = vk::IndexType::eUint16;
                Assert(primitive.indexBuffer.byteStride == 2, "index buffer must be tightly packed");
                break;
            }
            Assert(p.indexType != vk::IndexType::eNoneKHR, "unimplemented vertex index type");

            auto &indexBuffer = buffers[primitive.indexBuffer.bufferIndex];
            size_t indexBufferSize = primitive.indexBuffer.componentCount * primitive.indexBuffer.byteStride;
            Assert(primitive.indexBuffer.byteOffset + indexBufferSize <= indexBuffer.data.size(),
                   "indexes overflow buffer");

            p.indexBuffer = device.AllocateBuffer(indexBufferSize,
                                                  vk::BufferUsageFlagBits::eIndexBuffer,
                                                  VMA_MEMORY_USAGE_CPU_TO_GPU);

            void *data;
            p.indexBuffer.Map(&data);
            memcpy(data, &indexBuffer.data[primitive.indexBuffer.byteOffset], indexBufferSize);
            p.indexBuffer.Unmap();

            p.indexCount = primitive.indexBuffer.componentCount;

            auto &posAttr = primitive.attributes[0];
            auto &normalAttr = primitive.attributes[1];
            auto &uvAttr = primitive.attributes[2];

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
            p.vertexBuffer = device.AllocateBuffer(vertexBufferSize,
                                                   vk::BufferUsageFlagBits::eVertexBuffer,
                                                   VMA_MEMORY_USAGE_CPU_TO_GPU);

            p.vertexBuffer.Map(&data);
            memcpy(data, vertices.data(), vertexBufferSize);
            p.vertexBuffer.Unmap();
        }
    }

    Model::~Model() {
        Debugf("Destroying vulkan::Model %s", modelName);
    }

    void Model::AppendDrawCommands(const CommandContextPtr &cmd, glm::mat4 modelMat, const ecs::View &view) {
        for (auto &primitivePtr : primitives) {
            auto &primitive = *primitivePtr;
            MeshPushConstants constants;
            constants.projection = view.projMat;
            constants.view = view.viewMat;
            constants.model = modelMat * primitive.transform;

            cmd->PushConstants(&constants, 0, sizeof(MeshPushConstants));

            cmd->Raw().bindIndexBuffer(*primitive.indexBuffer, 0, primitive.indexType);
            cmd->Raw().bindVertexBuffers(0, {*primitive.vertexBuffer}, {0});
            cmd->DrawIndexed(primitive.indexCount, 1, 0, 0, 0);
        }
    }
} // namespace sp::vulkan
