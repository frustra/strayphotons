#include "Model.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "core/Logging.hh"

#include <filesystem>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <murmurhash/MurmurHash3.h>
#include <tiny_gltf.h>

namespace sp {
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

    Model::Attribute GetPrimitiveAttribute(std::shared_ptr<const tinygltf::Model> model,
                                           const tinygltf::Primitive *p,
                                           std::string attribute) {
        if (!p->attributes.count(attribute)) return Model::Attribute();
        auto accessor = model->accessors[p->attributes.at(attribute)];
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

        return Model::Attribute{accessor.byteOffset,
                                bufView.byteOffset,
                                accessor.byteOffset + bufView.byteOffset,
                                accessor.ByteStride(bufView),
                                accessor.componentType,
                                componentCount,
                                accessor.count,
                                bufView.buffer};
    }

    Model::~Model() {
        Debugf("Destroying model %s", name);
    }

    bool Model::HasBuffer(size_t index) const {
        Assert(valid.test(), "Checking buffer on invalid model");
        return model->buffers.size() > index;
    }

    const std::vector<unsigned char> &Model::GetBuffer(size_t index) const {
        Assert(valid.test(), "Accessing buffer on invalid model");
        return model->buffers[index].data;
    }

    Hash128 Model::HashBuffer(size_t index) const {
        Assert(valid.test(), "Hashing buffer on invalid model");
        Hash128 output;
        auto buffer = GetBuffer(index);
        MurmurHash3_x86_128(buffer.data(), buffer.size(), 0, output.data());
        return output;
    }

    // Returns a vector of the GLTF node indexes that are present in the "joints"
    // array of the GLTF skin.
    std::vector<int> Model::GetJointNodes() const {
        Assert(valid.test(), "Getting joint nodes on invalid model");
        std::vector<int> nodes;

        // TODO: deal with GLTFs that have more than one skin
        for (int node : model->skins[0].joints) {
            nodes.push_back(node);
        }

        return nodes;
    }

    int Model::FindNodeByName(std::string name) const {
        Assert(valid.test(), "Finding joint nodes on invalid model");
        for (size_t i = 0; i < model->nodes.size(); i++) {
            if (model->nodes[i].name == name) { return i; }
        }
        return -1;
    }

    glm::mat4 Model::GetInvBindPoseForNode(int nodeIndex) const {
        Assert(valid.test(), "Getting bind pose on invalid model");
        return inverseBindMatrixForJoint.at(nodeIndex);
    }

    std::string Model::GetNodeName(int node) const {
        Assert(valid.test(), "Getting node name on invalid model");
        return model->nodes[node].name;
    }

    void Model::PopulateFromAsset(std::shared_ptr<const Asset> asset, const tinygltf::FsCallbacks *fsCallbacks) {
        Assert(asset != nullptr, "Loading Model from null asset");
        this->asset = asset;
        asset->WaitUntilValid();

        std::filesystem::path fsPath(asset->path);
        std::string baseDir = fsPath.remove_filename().string();

        Debugf("Loading model %s from %s", name, asset->path);

        auto gltfModel = std::make_shared<tinygltf::Model>();
        std::string err;
        std::string warn;
        bool ret = false;
        tinygltf::TinyGLTF gltfLoader;
        if (fsCallbacks != nullptr) {
            gltfLoader.SetFsCallbacks(*fsCallbacks);
        } else {
            tinygltf::FsCallbacks fs = {};
            gltfLoader.SetFsCallbacks(fs);
        }
        if (ends_with(asset->path, ".gltf")) {
            ret = gltfLoader.LoadASCIIFromString(gltfModel.get(),
                                                 &err,
                                                 &warn,
                                                 (char *)asset->Buffer(),
                                                 asset->BufferSize(),
                                                 baseDir);
        } else {
            // Assume GLB model
            ret = gltfLoader.LoadBinaryFromMemory(gltfModel.get(),
                                                  &err,
                                                  &warn,
                                                  (unsigned char *)asset->Buffer(),
                                                  asset->BufferSize(),
                                                  baseDir);
        }

        Assert(err.empty(), "Failed to parse glTF: " + err);
        Assert(ret, "Failed to parse glTF");
        this->model = gltfModel;

        int defaultScene = 0;
        if (model->defaultScene != -1) { defaultScene = model->defaultScene; }

        for (int node : model->scenes[defaultScene].nodes) {
            AddNode(node, glm::mat4());
        }

        this->valid.test_and_set();
        this->valid.notify_all();
    }

    void Model::AddNode(int nodeIndex, glm::mat4 parentMatrix) {
        glm::mat4 matrix = parentMatrix * GetNodeMatrix(&model->nodes[nodeIndex]);
        auto &node = model->nodes[nodeIndex];

        // Meshes are optional on nodes
        if (node.mesh != -1) {
            for (auto primitive : model->meshes[node.mesh].primitives) {
                auto iAcc = model->accessors[primitive.indices];
                auto iBufView = model->bufferViews[iAcc.bufferView];

                DrawMode mode = DrawMode::Triangles;
                if (primitive.mode == TINYGLTF_MODE_TRIANGLES) {
                    mode = DrawMode::Triangles;
                } else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
                    mode = DrawMode::TriangleStrip;
                } else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_FAN) {
                    mode = DrawMode::TriangleFan;
                } else if (primitive.mode == TINYGLTF_MODE_POINTS) {
                    mode = DrawMode::Points;
                } else if (primitive.mode == TINYGLTF_MODE_LINE) {
                    mode = DrawMode::Line;
                } else if (primitive.mode == TINYGLTF_MODE_LINE_LOOP) {
                    mode = DrawMode::LineLoop;
                } else if (primitive.mode == TINYGLTF_MODE_LINE_STRIP) {
                    mode = DrawMode::LineStrip;
                }

                Assert(iAcc.type == TINYGLTF_TYPE_SCALAR, "index buffer type must be scalar");

                primitives.emplace_back(Primitive{matrix,
                                                  mode,
                                                  Attribute{iAcc.byteOffset,
                                                            iBufView.byteOffset,
                                                            iAcc.byteOffset + iBufView.byteOffset,
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
            if (node.skin != -1) {
                int skinId = node.skin;
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

                for (size_t i = 0; i < model->skins[skinId].joints.size(); i++) {
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

        for (int childNodeIndex : node.children) {
            AddNode(childNodeIndex, matrix);
        }
    }
} // namespace sp
