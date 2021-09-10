#pragma once

#include "graphics/opengl/Shader.hh"

#include <unordered_map>

namespace sp {
    class ShaderManager {
    public:
        static void RegisterShaderType(ShaderMeta *metaType);
        static vector<ShaderMeta *> &ShaderTypes();

        static void SetDefine(string name, string value);
        static void SetDefine(string name, bool value = true);
        static std::unordered_map<string, string> &DefineVars();

        ShaderManager(ShaderSet &shaders) : shaders(shaders) {}
        ~ShaderManager();
        void CompileAll(ShaderSet &shaders);

        void BindPipeline(vector<ShaderMeta *> shaderMetaTypes);

        template<typename... ShaderTypes>
        void BindPipeline() {
            BindPipeline({&ShaderTypes::MetaType...});
        }

        const ShaderSet &shaders;

    private:
        ShaderCompileInput LoadShader(ShaderMeta *shaderType);
        shared_ptr<ShaderCompileOutput> CompileShader(ShaderCompileInput &input);

        string LoadShader(ShaderCompileInput &input, string name);
        string ProcessShaderSource(ShaderCompileInput &input, const string &src, const string &path);
        string ProcessError(ShaderCompileInput &input, string err);

        std::unordered_map<size_t, GLuint> pipelineCache;
    };
} // namespace sp
