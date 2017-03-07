#pragma once

#include "graphics/Shader.hh"
#include <unordered_map>

namespace sp
{
	class ShaderManager
	{
	public:
		static void RegisterShaderType(ShaderMeta *metaType);
		static vector<ShaderMeta *> &ShaderTypes();

		static void SetDefine(string name, string value);
		static void SetDefine(string name, bool value = true);
		static std::unordered_map<string, string> &DefineVars();

		ShaderManager() { }
		~ShaderManager();
		void CompileAll(ShaderSet *shaders);

		void BindPipeline(ShaderSet *shaders, vector<ShaderMeta *> shaderMetaTypes);

		template <typename ...ShaderTypes>
		void BindPipeline(ShaderSet *shaders)
		{
			BindPipeline(shaders, { &ShaderTypes::MetaType... });
		}

	private:
		ShaderCompileInput LoadShader(ShaderMeta *shaderType);
		shared_ptr<ShaderCompileOutput> CompileShader(ShaderCompileInput &input);

		string LoadShader(ShaderCompileInput &input, string name);
		string ProcessShaderSource(ShaderCompileInput &input, string src);
		string ProcessError(ShaderCompileInput &input, string err);

		std::unordered_map<size_t, GLuint> pipelineCache;
	};
}
