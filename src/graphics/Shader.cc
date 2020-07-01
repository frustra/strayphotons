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
		for (auto b : buffers)
		{
			glDeleteBuffers(1, &b->handle);
		}
	}

	void Shader::Bind(Uniform &u, string name)
	{
		u.name = name;
		u.location = glGetUniformLocation(program, name.c_str());
		if (u.location == -1)
		{
			Debugf("Warning: Binding inactive uniform %s in shader %s: %s", name, type->name, type->filename);
		}
		AssertGLOK("glGetUniformLocation");
	}

	void Shader::BindBuffer(ShaderBuffer &b, int index, GLenum target, GLenum usage)
	{
		Assert(b.index == -1, "recreating shader buffer");

		b.index = index;
		b.target = target;
		b.usage = usage;
		b.size = 0;
		glCreateBuffers(1, &b.handle);
		AssertGLOK("glCreateBuffers");

		buffers.push_back(&b);
	}

	void Shader::BufferData(ShaderBuffer &b, GLsizei size, const void *data)
	{
		Assert(b.index != -1, "buffer not created");

		b.size = size;
		glNamedBufferData(b.handle, size, data, b.usage);
	}

	void Shader::BindBuffers()
	{
		for (auto b : buffers)
		{
			if (b->index != -1 && b->size > 0)
			{
				glBindBufferRange(b->target, b->index, b->handle, 0, b->size);
			}
		}
	}
}