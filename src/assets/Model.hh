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
    struct BasicMaterial;

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

    struct BasicMaterial {
        GLTexture baseColorTex, metallicRoughnessTex, heightTex;

        BasicMaterial(unsigned char *baseColor = nullptr,
                      unsigned char *metallicRoughness = nullptr,
                      unsigned char *bump = nullptr) {
            unsigned char baseColorDefault[4] = {255, 255, 255, 255};
            unsigned char metallicRoughnessDefault[4] = {0, 255, 0, 0}; // Roughness = G channel, Metallic = B channel
            unsigned char bumpDefault[4] = {127, 127, 127, 255};

            if (!baseColor) baseColor = baseColorDefault;
            if (!metallicRoughness) metallicRoughness = metallicRoughnessDefault;
            if (!bump) bump = bumpDefault;

            baseColorTex.Create()
                .Filter(GL_NEAREST, GL_NEAREST)
                .Wrap(GL_REPEAT, GL_REPEAT)
                .Size(1, 1)
                .Storage(PF_RGB8)
                .Image2D(baseColor);

            metallicRoughnessTex.Create()
                .Filter(GL_NEAREST, GL_NEAREST)
                .Wrap(GL_REPEAT, GL_REPEAT)
                .Size(1, 1)
                .Storage(PF_R8)
                .Image2D(metallicRoughness);

            heightTex.Create()
                .Filter(GL_NEAREST, GL_NEAREST)
                .Wrap(GL_REPEAT, GL_REPEAT)
                .Size(1, 1)
                .Storage(PF_R8)
                .Image2D(bump);
        }
    };
} // namespace sp