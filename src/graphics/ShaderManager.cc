#include "graphics/ShaderManager.hh"
#include "core/Logging.hh"

#include <fstream>

namespace sp
{
	vector<ShaderMeta *> &ShaderManager::ShaderTypes()
	{
		static vector<ShaderMeta *> shaderTypes;
		return shaderTypes;
	}

	void ShaderManager::RegisterShaderType(ShaderMeta *metaType)
	{
		ShaderTypes().push_back(metaType);
	}

	ShaderManager::~ShaderManager()
	{
		for (auto cached : pipelineCache)
			glDeleteProgramPipelines(1, &cached.second);
	}

	void ShaderManager::CompileAll(ShaderSet &shaders)
	{
		for (auto shaderType : ShaderTypes())
		{
			auto input = LoadShader(shaderType);
			auto output = CompileShader(input);
			if (!output) continue;

			output->shaderType = shaderType;
			auto shader = shaderType->newInstance(output);
			shared_ptr<Shader> shaderPtr(shader);
			shaders.shaders[shaderType] = shaderPtr;
		}
	}

	shared_ptr<ShaderCompileOutput> ShaderManager::CompileShader(ShaderCompileInput &input)
	{
		auto sourceStr = input.source.c_str();
		GLuint program = glCreateShaderProgramv(input.shaderType->GLStage(), 1, &sourceStr);
		Assert(program, "failed to create shader program");
		AssertGLOK("glCreateShaderProgramv");

		char infoLog[2048];
		glGetProgramInfoLog(program, 2048, NULL, infoLog);

		if (*infoLog)
		{
			Errorf("Failed to compile or link shader %s\n%s", input.shaderType->name.c_str(), infoLog);
			Assert(false, "shader build failed");
			return nullptr;
		}

		auto output = make_shared<ShaderCompileOutput>();
		output->shaderType = input.shaderType;
		output->program = program;
		return output;
	}

	ShaderCompileInput ShaderManager::LoadShader(ShaderMeta *shaderType)
	{
		string filename = "../src/shaders/" + shaderType->filename;

		std::ifstream fin(filename, std::ios::binary);
		if (!fin.good())
		{
			Errorf("Shader file %s could not be read", filename.c_str());
			throw std::runtime_error("missing shader: " + shaderType->filename);
		}

		string buffer((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
		return ShaderCompileInput {shaderType, buffer};
	}

	template <class T>
	inline void hash_combine(size_t &seed, const T &v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	void ShaderManager::BindPipeline(ShaderSet &shaders, vector<ShaderMeta *> shaderMetaTypes)
	{
		size_t hash = 0;

		for (auto shaderMeta : shaderMetaTypes)
		{
			auto shader = shaders.Get(shaderMeta);
			hash_combine(hash, shader->GLProgram());
		}

		auto cached = pipelineCache.find(hash);
		if (cached != pipelineCache.end())
		{
			glBindProgramPipeline(cached->second);
			return;
		}

		GLuint pipeline;
		glGenProgramPipelines(1, &pipeline);

		for (auto shaderMeta : shaderMetaTypes)
		{
			auto shader = shaders.Get(shaderMeta);
			glUseProgramStages(pipeline, shaderMeta->GLStageBits(), shader->GLProgram());
		}

		glBindProgramPipeline(pipeline);
		pipelineCache[hash] = pipeline;
	}
}
