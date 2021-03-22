#pragma once

#include "Common.hh" // For NonCopyable
#include "graphics/Graphics.hh"
#include "graphics/Renderer.hh"
#include "graphics/SceneShaders.hh"
#include "graphics/Texture.hh"
#include "assets/Model.hh"

#include <map>

namespace sp {
    enum TextureType;

    class GLModel : public NonCopyable {
    public:
        GLModel(Model *model, Renderer *renderer);
        ~GLModel();

        struct Primitive {
            Model::Primitive *parent;
            GLuint vertexBufferHandle;
            GLuint indexBufferHandle;
            GLuint weightsBufferHandle;
            GLuint jointsBufferHandle;
            Texture *baseColorTex, *metallicRoughnessTex, *heightTex;
        };

        void Draw(SceneShader *shader, glm::mat4 modelMat, const ecs::View &view, int boneCount, glm::mat4 *boneData);

        void AddPrimitive(GLModel::Primitive prim);

    private:
        GLuint LoadBuffer(int index);
        Texture *LoadTexture(int materialIndex, TextureType type);

        Model *model;
        Renderer *renderer;
        std::map<int, GLuint> buffers;
        std::map<std::string, Texture> textures;
        vector<GLModel::Primitive> primitives;
    };
};