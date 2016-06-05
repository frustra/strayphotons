#pragma once

#include "Common.hh"
#include "Graphics.hh"
#include <unordered_map>

#define SHADER_TYPE(ClassName) \
	public: \
	static ShaderMeta MetaType; \
	static Shader *NewInstance(shared_ptr<ShaderCompileOutput> compileOutput) \
	{ return new ClassName(compileOutput); }

#define IMPLEMENT_SHADER_TYPE(cls, file, stage) \
	ShaderMeta cls::MetaType{#cls, file, ShaderStage::stage, cls::NewInstance}

namespace sp
{
	class Shader;
	class ShaderCompileOutput;

	enum class ShaderStage
	{
		Vertex = GL_VERTEX_SHADER,
		Fragment = GL_FRAGMENT_SHADER,
		Compute = GL_COMPUTE_SHADER,
		TessControl = GL_TESS_CONTROL_SHADER,
		TessEval = GL_TESS_EVALUATION_SHADER,
	};

	class ShaderMeta
	{
	public:
		typedef Shader *(*Constructor)(shared_ptr<ShaderCompileOutput>);

		ShaderMeta(string name, string filename, ShaderStage stage, Constructor newInstance);

		const string name;
		const string filename;
		const ShaderStage stage;

		const GLbitfield GLStageBits()
		{
			switch (stage)
			{
				case ShaderStage::Vertex:
					return GL_VERTEX_SHADER_BIT;
				case ShaderStage::Fragment:
					return GL_FRAGMENT_SHADER_BIT;
				case ShaderStage::Compute:
					return GL_COMPUTE_SHADER_BIT;
				case ShaderStage::TessControl:
					return GL_TESS_CONTROL_SHADER_BIT;
				case ShaderStage::TessEval:
					return GL_TESS_EVALUATION_SHADER_BIT;
				default:
					return 0;
			}
		}

		const GLenum GLStage()
		{
			return (GLenum)stage;
		}

		Constructor newInstance;
	};

	class ShaderCompileInput
	{
	public:
		ShaderMeta *shaderType;
		string source;
		vector<string> units;
	};

	class ShaderCompileOutput
	{
	public:
		ShaderMeta *shaderType;
		GLuint program;

	private:
	};

	struct Uniform
	{
		string name;
		GLint location;
	};

	class Shader
	{
	public:
		Shader(shared_ptr<ShaderCompileOutput> compileOutput);
		virtual ~Shader();

		std::unordered_map<string, Uniform> uniforms;

		const GLuint GLProgram()
		{
			return compileOutput->program;
		}

	protected:
		void Bind(Uniform &u, string name);

		// Methods for setting uniform values.

		template <typename ValueType>
		void Set(Uniform u, const ValueType &v)
		{
			Assert(false, "unimplemented uniform type");
		}

		template <typename ValueType>
		void Set(Uniform u, const ValueType &v, GLsizei count)
		{
			Assert(false, "unimplemented uniform type");
		}

	private:
		ShaderMeta *type;
		GLuint program;
		shared_ptr<ShaderCompileOutput> compileOutput;
	};

	class ShaderSet
	{
	public:
		typedef std::unordered_map<ShaderMeta *, shared_ptr<Shader>> MapType;

		const MapType &Map() const
		{
			return shaders;
		}

		shared_ptr<Shader> Get(ShaderMeta *meta) const
		{
			auto ptr = shaders.find(meta);

			if (ptr == shaders.end())
				throw std::runtime_error("shader not loaded: " + meta->name);

			return ptr->second;
		}

		template <typename ShaderType>
		ShaderType *Get()
		{
			return static_cast<ShaderType *>(Get(&ShaderType::MetaType).get());
		}

	private:
		MapType shaders;

		friend class ShaderManager;
	};

#define DECLARE_SET_SCALAR(UniformSuffix, ValueType) \
	template <> inline void Shader::Set<ValueType>(Uniform u, const ValueType &v) { \
		glProgramUniform##UniformSuffix(program, u.location, v); \
	}

#define DECLARE_SET_GLM(UniformSuffix, ValueType) \
	template <> inline void Shader::Set<ValueType>(Uniform u, const ValueType &v) { \
		glProgramUniform##UniformSuffix(program, u.location, 1, glm::value_ptr(v)); \
	} \
	template <> inline void Shader::Set<ValueType>(Uniform u, const ValueType &v, GLsizei count) { \
		glProgramUniform##UniformSuffix(program, u.location, count, glm::value_ptr(v)); \
	}

#define DECLARE_SET_GLM_MAT(UniformSuffix, ValueType) \
	template <> inline void Shader::Set<ValueType>(Uniform u, const ValueType &v) { \
		glProgramUniformMatrix##UniformSuffix(program, u.location, 1, 0, glm::value_ptr(v)); \
	} \
	template <> inline void Shader::Set<ValueType>(Uniform u, const ValueType &v, GLsizei count) { \
		glProgramUniformMatrix##UniformSuffix(program, u.location, count, 0, glm::value_ptr(v)); \
	}

	DECLARE_SET_SCALAR(1f, float)
	DECLARE_SET_SCALAR(1i, int32)
	DECLARE_SET_SCALAR(1ui, uint32)
	DECLARE_SET_GLM(2fv, glm::vec2)
	DECLARE_SET_GLM(3fv, glm::vec3)
	DECLARE_SET_GLM(4fv, glm::vec4)
	DECLARE_SET_GLM(2iv, glm::i32vec2)
	DECLARE_SET_GLM(3iv, glm::i32vec3)
	DECLARE_SET_GLM(4iv, glm::i32vec4)
	DECLARE_SET_GLM(2uiv, glm::u32vec2)
	DECLARE_SET_GLM(3uiv, glm::u32vec3)
	DECLARE_SET_GLM(4uiv, glm::u32vec4)
	DECLARE_SET_GLM_MAT(2fv, glm::mat2)
	DECLARE_SET_GLM_MAT(3fv, glm::mat3)
	DECLARE_SET_GLM_MAT(4fv, glm::mat4)

#undef DECLARE_SET_SCALAR
#undef DECLARE_SET_GLM
#undef DECLARE_SET_GLM_MAT
}
