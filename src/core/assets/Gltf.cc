#include "Gltf.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/GltfImpl.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"

#include <filesystem>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <tiny_gltf.h>

namespace sp {
    namespace gltf {
        Mesh::Primitive::Primitive(const tinygltf::Model &model, const tinygltf::Primitive &primitive) {
            if (primitive.mode == TINYGLTF_MODE_TRIANGLES) {
                drawMode = DrawMode::Triangles;
            } else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
                drawMode = DrawMode::TriangleStrip;
            } else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_FAN) {
                drawMode = DrawMode::TriangleFan;
            } else if (primitive.mode == TINYGLTF_MODE_POINTS) {
                drawMode = DrawMode::Points;
            } else if (primitive.mode == TINYGLTF_MODE_LINE) {
                drawMode = DrawMode::Line;
            } else if (primitive.mode == TINYGLTF_MODE_LINE_LOOP) {
                drawMode = DrawMode::LineLoop;
            } else if (primitive.mode == TINYGLTF_MODE_LINE_STRIP) {
                drawMode = DrawMode::LineStrip;
            } else {
                drawMode = DrawMode::Triangles;
            }

            indexBuffer = Accessor<uint32_t, uint16_t>(model, primitive.indices);
            materialIndex = primitive.material;
            auto it = primitive.attributes.find("POSITION");
            if (it != primitive.attributes.end()) positionBuffer = Accessor<glm::vec3>(model, it->second);
            it = primitive.attributes.find("NORMAL");
            if (it != primitive.attributes.end()) normalBuffer = Accessor<glm::vec3>(model, it->second);
            it = primitive.attributes.find("TEXCOORD_0");
            if (it != primitive.attributes.end()) {
                texcoordBuffer = Accessor<glm::vec2, glm::u16vec2, glm::u8vec2, glm::i16vec2, glm::i8vec2>(model,
                    it->second);
            }
            it = primitive.attributes.find("JOINTS_0");
            if (it != primitive.attributes.end()) {
                jointsBuffer = Accessor<glm::u16vec4, glm::u8vec4>(model, it->second);
            }
            it = primitive.attributes.find("WEIGHTS_0");
            if (it != primitive.attributes.end()) {
                weightsBuffer = Accessor<glm::vec4, glm::u16vec4, glm::u8vec4>(model, it->second);
            }
        }

        Mesh::Mesh(const tinygltf::Model &model, const tinygltf::Mesh &mesh) {
            for (auto &primitive : mesh.primitives) {
                primitives.emplace_back(model, primitive);
            }
        }

        Skin::Skin(const tinygltf::Model &model, const tinygltf::Skin &skin) {
            Accessor<glm::mat4> inverseBindMatrices(model, skin.inverseBindMatrices);

            joints.resize(skin.joints.size());
            for (size_t i = 0; i < skin.joints.size(); i++) {
                joints[i].jointNodeIndex = skin.joints[i];
                if (i < inverseBindMatrices.Count()) {
                    joints[i].inverseBindPose = inverseBindMatrices.Read(i);
                } else {
                    joints[i].inverseBindPose = glm::identity<glm::mat4>();
                }
            }
            rootJoint = skin.skeleton;
        }
    } // namespace gltf

    glm::mat4 GetNodeMatrix(const tinygltf::Node *node) {
        glm::mat4 out(1.0);

        if (node->matrix.size() == 16) {
            auto outPtr = glm::value_ptr(out);
            for (auto value : node->matrix) {
                *(outPtr++) = (float)value;
            }
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

    Gltf::Gltf(const string &name, std::shared_ptr<const Asset> asset) : name(name), asset(asset) {
        Assertf(asset, "Gltf not found: %s", name);

        ZoneScopedN("LoadGltf");
        ZonePrintf("%s from %s", name, asset->path);

        std::filesystem::path fsPath(asset->path);
        std::string baseDir = fsPath.remove_filename().string();

        auto gltfModel = std::make_shared<tinygltf::Model>();
        std::string err;
        std::string warn;
        bool ret = false;
        tinygltf::TinyGLTF gltfLoader;
        Assert(asset->BufferSize() <= UINT_MAX, "Buffer size overflows max uint");
        if (ends_with(asset->path, ".gltf")) {
            ret = gltfLoader.LoadASCIIFromString(gltfModel.get(),
                &err,
                &warn,
                (char *)asset->BufferPtr(),
                (uint32_t)asset->BufferSize(),
                baseDir);
        } else {
            // Assume GLB model
            ret = gltfLoader.LoadBinaryFromMemory(gltfModel.get(),
                &err,
                &warn,
                asset->BufferPtr(),
                (uint32_t)asset->BufferSize(),
                baseDir);
        }

        Assertf(ret && err.empty(), "Failed to parse glTF (%s): %s", name, err);
        this->model = gltfModel;

        int sceneIndex = gltfModel->defaultScene >= 0 ? gltfModel->defaultScene : 0;
        Assertf(sceneIndex < gltfModel->scenes.size(), "Gltf scene index is out of range: %d", sceneIndex);
        auto &scene = gltfModel->scenes[sceneIndex];

        nodes.reserve(scene.nodes.size());
        for (auto &node : scene.nodes) {
            nodes.emplace_back(*gltfModel, node);
        }
    }

    void Gltf::AddNode(int nodeIndex, glm::mat4 parentMatrix) {
        glm::mat4 matrix = parentMatrix * GetNodeMatrix(&model.nodes[nodeIndex]);
        auto &node = model.nodes[nodeIndex];

        // Meshes are optional on nodes
        if (node.mesh != -1) {

            // Must have a mesh to have a skin
            if (node.skin != -1) {}
        }

        for (int childNodeIndex : node.children) {
            AddNode(childNodeIndex, matrix);
        }
    }
} // namespace sp
