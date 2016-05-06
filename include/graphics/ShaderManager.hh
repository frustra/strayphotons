#ifndef SP_SHADER_MANAGER_H
#define SP_SHADER_MANAGER_H

#include "graphics/Shader.hh"
#include <unordered_map>

namespace sp
{
	class ShaderManager
	{
	public:
		static void RegisterShaderType(ShaderMeta *metaType);
		static vector<ShaderMeta *> &ShaderTypes();

		ShaderManager() { }
		~ShaderManager();
		void CompileAll(ShaderSet &shaders);

		void BindPipeline(ShaderSet &shaders, vector<ShaderMeta *> shaderMetaTypes);

		template <typename ...ShaderTypes>
		void BindPipeline(ShaderSet &shaders)
		{
			BindPipeline(shaders, { &ShaderTypes::MetaType... });
		}

	private:
		ShaderCompileInput LoadShader(ShaderMeta *shaderType);
		shared_ptr<ShaderCompileOutput> CompileShader(ShaderCompileInput &input);

		std::unordered_map<size_t, GLuint> pipelineCache;
	};
}
#endif