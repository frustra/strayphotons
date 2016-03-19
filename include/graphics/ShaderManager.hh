//=== Copyright Frustra Software, all rights reserved ===//

#ifndef SP_SHADER_MANAGER_H
#define SP_SHADER_MANAGER_H

#include "graphics/Shader.hh"
#include "graphics/Device.hh"

namespace sp
{
	class ShaderManager
	{
	public:
		static void RegisterShaderType(ShaderMeta *metaType);
		static vector<ShaderMeta *> &ShaderTypes();

		ShaderManager(Device &device) : device(device) { }
		~ShaderManager();
		void CompileAll(ShaderSet &shaders);

	private:
		ShaderCompileInput LoadShader(ShaderMeta *shaderType);
		shared_ptr<ShaderCompileOutput> CompileShader(ShaderCompileInput &input);
		void ParseShader(ShaderCompileInput &input, ShaderCompileOutput *output);

		Device &device;
	};
}
#endif