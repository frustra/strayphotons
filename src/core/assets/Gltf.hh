#pragma once

#include "core/Common.hh"
#include "ecs/components/Transform.h"

#include <glm/glm.hpp>
#include <optional>
#include <vector>

namespace tinygltf {
    class Model;
    struct Accessor;
    struct Mesh;
    struct Primitive;
    struct Skin;
    class Node;
    struct Buffer;
} // namespace tinygltf

namespace sp {
    class Asset;
    class Gltf;

    enum class TextureType {
        BaseColor,
        MetallicRoughness,
        Height,
        Occlusion,
        Emissive,
    };

    namespace gltf {
        template<typename ReadT, typename... Tn>
        class Accessor {
        public:
            Accessor() {}
            Accessor(const tinygltf::Model &model, int accessorIndex);
            Accessor(const Gltf &model, int accessorIndex);

            explicit operator bool() const {
                return buffer && typeIndex >= 0;
            }

            size_t Count() const;

            ReadT Read(size_t i) const;

        private:
            const tinygltf::Buffer *buffer = nullptr;
            int typeIndex = -1;
            size_t count = 0;
            size_t componentCount = 0;
            size_t byteOffset = 0;
            size_t byteStride = 0;
        };

        struct Mesh {
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
                Primitive(const tinygltf::Model &model, const tinygltf::Primitive &primitive);

                DrawMode drawMode;
                Accessor<uint32_t, uint16_t> indexBuffer;
                int materialIndex;
                Accessor<glm::vec3> positionBuffer;
                Accessor<glm::vec3> normalBuffer;
                Accessor<glm::vec2 /*, glm::u16vec2, glm::u8vec2, glm::i16vec2, glm::i8vec2*/> texcoordBuffer;
                Accessor<glm::u16vec4, glm::u8vec4> jointsBuffer;
                Accessor<glm::vec4 /*, glm::u16vec4, glm::u8vec4*/> weightsBuffer;
            };

            Mesh(const tinygltf::Model &model, const tinygltf::Mesh &mesh);

            std::vector<Primitive> primitives;
        };

        struct Joint {
            size_t jointNodeIndex;
            glm::mat4 inverseBindPose;
        };

        struct Skin {
            Skin(const tinygltf::Model &model, const tinygltf::Skin &skin);

            std::vector<Joint> joints;
            size_t rootJoint;
        };

        struct Node {
            Node(const tinygltf::Model &model,
                const tinygltf::Node &node,
                std::optional<size_t> treeRoot = std::optional<size_t>());

            std::string name;
            ecs::Transform transform;

            std::optional<size_t> treeRoot;
            std::vector<size_t> children;
            std::optional<size_t> meshIndex;
            std::optional<size_t> skinIndex;
        };
    } // namespace gltf

    class Gltf : public NonCopyable {
    public:
        Gltf(const string &name, std::shared_ptr<const Asset> asset);

        const std::string name;

        std::shared_ptr<const Asset> asset;
        std::shared_ptr<const tinygltf::Model> gltfModel;
        std::vector<std::optional<gltf::Node>> nodes;
        std::vector<std::optional<gltf::Skin>> skins;
        std::vector<std::optional<gltf::Mesh>> meshes;

        std::vector<size_t> rootNodes;

    private:
        bool AddNode(const tinygltf::Model &model,
            int nodeIndex,
            std::optional<size_t> treeRoot = std::optional<size_t>());
    };
} // namespace sp
