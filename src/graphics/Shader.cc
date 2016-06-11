#include "graphics/Shader.hh"
#include "graphics/ShaderManager.hh"
#include "core/Logging.hh"

namespace sp
{
	ShaderMeta::ShaderMeta(string name, string filename, ShaderStage stage, Constructor newInstance)
		: name(name), filename(filename), stage(stage), newInstance(newInstance)
	{
		ShaderManager::RegisterShaderType(this);
	}

	Shader::Shader(shared_ptr<ShaderCompileOutput> compileOutput)
		: type(compileOutput->shaderType), program(compileOutput->program), compileOutput(compileOutput)
	{

	}

	Shader::~Shader()
	{
		for (auto & entry : uniforms)
		{
			auto &unif = entry.second;
		}

	}

	void Shader::Bind(Uniform &u, string name)
	{
		u.name = name;
		u.location = glGetUniformLocation(program, name.c_str());
		AssertGLOK("glGetUniformLocation");
	}
}