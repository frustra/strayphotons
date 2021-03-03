#pragma once

#include "Common.hh"
#include "graphics/opengl/GLTexture.hh"

#include <array>
#include <tinygltf/tiny_gltf.h>

namespace sp {
    // Forward declarations
    class Asset;
    class GLModel;
    class Renderer;

    typedef std::array<uint32, 4> Hash128;

    enum TextureType {
        BaseColor,
        MetallicRoughness,
        Height,
        Occlusion,
        Emissive,
    };

    class Model : public NonCopyable {
        friend GLModel;

    public:
        Model(const string &name) : name(name){};
        Model(const string &name, shared_ptr<Asset> asset, shared_ptr<tinygltf::Model> model) : Model(name, model) {
            this->asset = asset;
        };
        Model(const string &name, shared_ptr<tinygltf::Model> model);

        virtual ~Model();

        struct Attribute {
            size_t byteOffset;
            int byteStride;
            int componentType;
            size_t componentCount;
            size_t components;
            int bufferIndex;
        };

        struct Primitive {
            glm::mat4 matrix;
            int drawMode;
            Attribute indexBuffer;
            int materialIndex;
            std::array<Attribute, 5> attributes;
        };

        const string name;
        shared_ptr<GLModel> glModel;
        vector<Primitive *> primitives;

        bool HasBuffer(int index);
        const vector<unsigned char> &GetBuffer(int index);
        Hash128 HashBuffer(int index);

        int FindNodeByName(string name);
        string GetNodeName(int node);
        glm::mat4 GetInvBindPoseForNode(int nodeIndex);
        vector<int> GetJointNodes();

        vector<glm::mat4> bones;

    private:
        void AddNode(int nodeIndex, glm::mat4 parentMatrix);

        shared_ptr<tinygltf::Model> model;
        shared_ptr<Asset> asset;

        // TODO: support more than one "skin" in a GLTF
        std::map<int, glm::mat4> inverseBindMatrixForJoint;
        int rootBone;
    };
} // namespace sp