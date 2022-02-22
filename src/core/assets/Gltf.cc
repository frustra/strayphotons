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

        Node::Node(const tinygltf::Model &model, const tinygltf::Node &node, std::optional<size_t> treeRoot)
            : name(node.name), treeRoot(treeRoot) {
            if (node.matrix.size() >= 12) {
                auto out = glm::value_ptr(transform.matrix);
                for (size_t i = 0; i < 12; i++) {
                    out[i] = (float)node.matrix[i];
                }
            } else {
                if (node.translation.size() == 3) {
                    transform.SetPosition(glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
                }

                if (node.rotation.size() == 4) {
                    transform.SetRotation(
                        glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]));
                }

                if (node.scale.size() == 3) {
                    transform.SetScale(glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
                }
            }
        }
    } // namespace gltf

    Gltf::Gltf(const string &name, std::shared_ptr<const Asset> asset) : name(name), asset(asset) {
        Assertf(asset, "Gltf not found: %s", name);

        ZoneScopedN("LoadGltf");
        ZonePrintf("%s from %s", name, asset->path);

        std::filesystem::path fsPath(asset->path);
        std::string baseDir = fsPath.remove_filename().string();

        auto model = std::make_shared<tinygltf::Model>();
        std::string err;
        std::string warn;
        bool ret = false;
        tinygltf::TinyGLTF gltfLoader;
        Assert(asset->BufferSize() <= UINT_MAX, "Buffer size overflows max uint");
        if (ends_with(asset->path, ".gltf")) {
            ret = gltfLoader.LoadASCIIFromString(model.get(),
                &err,
                &warn,
                (char *)asset->BufferPtr(),
                (uint32_t)asset->BufferSize(),
                baseDir);
        } else {
            // Assume GLB model
            ret = gltfLoader.LoadBinaryFromMemory(model.get(),
                &err,
                &warn,
                asset->BufferPtr(),
                (uint32_t)asset->BufferSize(),
                baseDir);
        }

        Assertf(ret && err.empty(), "Failed to parse glTF (%s): %s", name, err);
        gltfModel = model;
        nodes.resize(model->nodes.size());
        skins.resize(model->skins.size());
        meshes.resize(model->meshes.size());

        int sceneIndex = model->defaultScene >= 0 ? model->defaultScene : 0;
        Assertf((size_t)sceneIndex < model->scenes.size(), "Gltf scene index is out of range: %d", sceneIndex);
        auto &scene = model->scenes[sceneIndex];

        for (int node : scene.nodes) {
            if (AddNode(*model, node)) rootNodes.push_back((size_t)node);
        }
    }

    bool Gltf::AddNode(const tinygltf::Model &model, int nodeIndex, std::optional<size_t> treeRoot) {
        if (nodeIndex < 0 || (size_t)nodeIndex >= nodes.size() || (size_t)nodeIndex >= model.nodes.size()) return false;
        auto &node = nodes[nodeIndex];
        if (node) {
            Errorf("Gltf nodes contain loop: %s %d", name, nodeIndex);
            return false;
        }
        node = gltf::Node(model, model.nodes[nodeIndex], treeRoot);

        auto meshIndex = model.nodes[nodeIndex].mesh;
        if (meshIndex >= 0 && (size_t)meshIndex < meshes.size() && (size_t)meshIndex < model.meshes.size()) {
            node->meshIndex = (size_t)meshIndex;
            if (!meshes[meshIndex]) { meshes[meshIndex] = gltf::Mesh(model, model.meshes[meshIndex]); }
        }

        auto skinIndex = model.nodes[nodeIndex].mesh;
        if (skinIndex >= 0 && (size_t)skinIndex < skins.size() && (size_t)skinIndex < model.skins.size()) {
            node->skinIndex = (size_t)skinIndex;
            if (!skins[skinIndex]) { skins[skinIndex] = gltf::Skin(model, model.skins[skinIndex]); }
        }

        for (int child : model.nodes[nodeIndex].children) {
            if (AddNode(model, child, treeRoot.value_or((size_t)nodeIndex))) node->children.push_back((size_t)child);
        }
        return true;
    }
} // namespace sp
