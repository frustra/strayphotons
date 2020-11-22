#pragma once

#include "graphics/GraphicsContext.hh"
#include "graphics/Shader.hh"

#include <unordered_map>

namespace sp {
    class ShaderManager {
    public:
        static void RegisterShaderType(ShaderMeta *metaType);
        static vector<ShaderMeta *> &ShaderTypes();

        static void SetDefine(string name, string value);
        static void SetDefine(string name, bool value = true);
        static std::unordered_map<string, string> &DefineVars();

        ShaderManager(ShaderSet *shaders) : activeShaders(shaders) {}
        ~ShaderManager();
        void CompileAll(ShaderSet *shaders = nullptr);

        void BindPipeline(const ShaderSet *shaders, vector<ShaderMeta *> shaderMetaTypes);

        template<typename... ShaderTypes>
        void BindPipeline(const ShaderSet *shaders = nullptr) {
            BindPipeline(shaders == nullptr ? activeShaders : shaders, {&ShaderTypes::MetaType...});
        }

        const ShaderSet *activeShaders;

    private:
        ShaderCompileInput LoadShader(ShaderMeta *shaderType);
        shared_ptr<ShaderCompileOutput> CompileShader(ShaderCompileInput &input);

        string LoadShader(ShaderCompileInput &input, string name);
        string ProcessShaderSource(ShaderCompileInput &input, string src);
        string ProcessError(ShaderCompileInput &input, string err);

        std::unordered_map<size_t, GLuint> pipelineCache;
    };
} // namespace sp
