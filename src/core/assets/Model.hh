#pragma once

#include "core/Common.hh"

#include <array>
#include <glm/glm.hpp>
#include <tinygltf/tiny_gltf.h>

namespace sp {

    // Forward declarations
    class Asset;

    typedef std::array<uint32, 4> Hash128;

    enum TextureType {
        BaseColor,
        MetallicRoughness,
        Height,
        Occlusion,
        Emissive,
    };

    class Model : public NonCopyable {

    public:
        Model(const string &name) : name(name){};
        Model(const string &name, shared_ptr<const Asset> asset, shared_ptr<tinygltf::Model> model)
            : Model(name, model) {
            this->asset = asset;
        };
        Model(const string &name, shared_ptr<tinygltf::Model> model);

        virtual ~Model();

        struct Attribute {
            size_t vertexByteOffset;
            size_t bufferByteOffset;
            size_t byteOffset; // equal to vertexByteOffset + bufferByteOffset
            int byteStride;
            int componentType;
            size_t componentFields;
            size_t componentCount;
            int bufferIndex;
        };

        enum class DrawMode {
            Points = TINYGLTF_MODE_POINTS,
            Line = TINYGLTF_MODE_LINE,
            LineLoop = TINYGLTF_MODE_LINE_LOOP,
            LineStrip = TINYGLTF_MODE_LINE_STRIP,
            Triangles = TINYGLTF_MODE_TRIANGLES,
            TriangleStrip = TINYGLTF_MODE_TRIANGLE_STRIP,
            TriangleFan = TINYGLTF_MODE_TRIANGLE_FAN,
        };

        struct Primitive {
            glm::mat4 matrix;
            DrawMode drawMode;
            Attribute indexBuffer;
            int materialIndex;
            std::array<Attribute, 5> attributes;
        };

        const string name;
        vector<Primitive> primitives;

        bool HasBuffer(size_t index) const;
        const std::vector<unsigned char> &GetBuffer(size_t index) const;
        Hash128 HashBuffer(size_t index) const;

        int FindNodeByName(string name) const;
        string GetNodeName(int node) const;
        glm::mat4 GetInvBindPoseForNode(int nodeIndex) const;
        std::vector<int> GetJointNodes() const;

        std::shared_ptr<const tinygltf::Model> GetModel() const {
            return model;
        }

        vector<glm::mat4> bones;

    private:
        void AddNode(int nodeIndex, glm::mat4 parentMatrix);

        std::shared_ptr<tinygltf::Model> model;
        std::shared_ptr<const Asset> asset;

        // TODO: support more than one "skin" in a GLTF
        std::map<int, glm::mat4> inverseBindMatrixForJoint;
        int rootBone;
    };
} // namespace sp
