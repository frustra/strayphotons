#pragma once

#include "assets/Model.hh"
#include "core/Common.hh"
#include "graphics/core/NativeModel.hh"
#include "graphics/core/Renderer.hh"
#include "graphics/opengl/GLTexture.hh"
#include "graphics/opengl/Graphics.hh"
#include "graphics/opengl/SceneShaders.hh"

#include <map>

namespace sp {
    enum TextureType;

    class GLModel final : public NonCopyable, public NativeModel {
    public:
        GLModel(Model *model, Renderer *renderer);
        ~GLModel();

        struct Primitive {
            Model::Primitive *parent;
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
                  glm::mat4 *boneData) override;

        void AddPrimitive(GLModel::Primitive prim);

        static GLenum GetDrawMode(Model::DrawMode mode);

    private:
        GLuint LoadBuffer(int index);
        GLTexture *LoadTexture(int materialIndex, TextureType type);

        std::map<int, GLuint> buffers;
        std::map<std::string, GLTexture> textures;
        vector<GLModel::Primitive> primitives;
    };
}; // namespace sp
