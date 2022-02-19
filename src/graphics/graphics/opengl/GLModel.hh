#pragma once

#include "assets/Model.hh"
#include "core/Common.hh"
#include "graphics/opengl/GLTexture.hh"
#include "graphics/opengl/Graphics.hh"
#include "graphics/opengl/SceneShaders.hh"

#include <map>

namespace sp {
    class VoxelRenderer;

    class GLModel final : public NonCopyable {
    public:
        GLModel(const std::shared_ptr<const Model> &model, VoxelRenderer *renderer);
        ~GLModel();

        struct Primitive : public Model::Primitive {
            Primitive(const Model::Primitive &parent) : Model::Primitive(parent) {}

            GLuint vertexBufferHandle;
            GLuint indexBufferHandle;
            GLuint weightsBufferHandle;
            GLuint jointsBufferHandle;
            GLTexture *baseColorTex, *metallicRoughnessTex, *heightTex;
            GLenum drawMode;
        };

        void Draw(SceneShader *shader,
            glm::mat4 modelMat,
            const ecs::View &view,
            int boneCount,
            const glm::mat4 *boneData) const;

        void AddPrimitive(GLModel::Primitive &prim);

        static GLenum GetDrawMode(Model::DrawMode mode);

    private:
        GLuint LoadBuffer(int index);
        GLTexture *LoadTexture(int materialIndex, TextureType type);

        VoxelRenderer *renderer;

        std::map<int, GLuint> buffers;
        std::map<std::string, GLTexture> textures;
        vector<GLModel::Primitive> primitives;

        std::shared_ptr<const Model> model;
    };
}; // namespace sp
