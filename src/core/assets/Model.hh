#pragma once

#include "core/Common.hh"

#include <array>
#include <atomic>
#include <glm/glm.hpp>
#include <map>

namespace tinygltf {
    class FsCallbacks;
    class Model;
} // namespace tinygltf

namespace sp {
    class Asset;

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
        Model(const string &name, std::shared_ptr<const Asset> asset) : name(name), asset(asset){};

        virtual ~Model();

        bool Valid() const {
            return valid.test();
        }

        void WaitUntilValid() const {
            while (!valid.test()) {
                valid.wait(false);
            }
        }

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
            Points = 0,
            Line,
            LineLoop,
            LineStrip,
            Triangles,
            TriangleStrip,
            TriangleFan,
        };

        struct Primitive {
            glm::mat4 matrix;
            DrawMode drawMode;
            Attribute indexBuffer;
            int materialIndex;
            std::array<Attribute, 5> attributes;
        };

        const std::string name;

        bool HasBuffer(size_t index) const;
        const std::vector<unsigned char> &GetBuffer(size_t index) const;
        Hash128 HashBuffer(size_t index) const;

        int FindNodeByName(std::string name) const;
        std::string GetNodeName(int node) const;
        glm::mat4 GetInvBindPoseForNode(int nodeIndex) const;
        std::vector<int> GetJointNodes() const;

        const std::shared_ptr<const tinygltf::Model> &GetGltfModel() const {
            Assert(valid.test(), "Accessing gltf on invalid model");
            return model;
        }

        const std::vector<Primitive> &Primitives() const {
            Assert(valid.test(), "Accessing primitives on invalid model");
            return primitives;
        }

        const std::vector<glm::mat4> &Bones() const {
            Assert(valid.test(), "Accessing bones on invalid model");
            return bones;
        }

    private:
        void PopulateFromAsset(std::shared_ptr<const Asset> asset, const tinygltf::FsCallbacks *fsCallbacks = nullptr);
        void AddNode(int nodeIndex, glm::mat4 parentMatrix);

        std::atomic_flag valid;
        std::shared_ptr<const Asset> asset;

        std::shared_ptr<const tinygltf::Model> model;
        std::vector<Primitive> primitives;
        std::vector<glm::mat4> bones;

        // TODO: support more than one "skin" in a GLTF
        std::map<int, glm::mat4> inverseBindMatrixForJoint;
        int rootBone;

        friend class AssetManager;
    };
} // namespace sp
