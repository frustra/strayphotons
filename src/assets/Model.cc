#include "assets/Model.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "core/Logging.hh"

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
        Debugf("Destroying model %s (prepared: %d)", name, !!nativeModel);
        for (auto primitive : primitives) {
            delete primitive;
        }

        if (!!asset) { asset->manager->UnregisterModel(*this); }
    }

    bool Model::HasBuffer(size_t index) {
        return model->buffers.size() > index;
    }

    const vector<unsigned char> &Model::GetBuffer(size_t index) {
        return model->buffers[index].data;
    }

    Hash128 Model::HashBuffer(size_t index) {
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
        for (size_t i = 0; i < model->nodes.size(); i++) {
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
} // namespace sp
