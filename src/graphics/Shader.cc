//=== Copyright Frustra Software, all rights reserved ===//

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
		: type(compileOutput->shaderType), compileOutput(compileOutput), graphics(*compileOutput->graphics)
	{

	}

	void Shader::Bind(void *ptr, size_t size, string name)
	{
		vk::BufferCreateInfo uboInfo;
		uboInfo.size(size);
		uboInfo.usage(vk::BufferUsageFlagBits::eUniformBuffer);

		auto &unif = uniforms[name];
		unif.buf = graphics.vkdev.createBuffer(uboInfo, nalloc);
		unif.mem = graphics.devmem->AllocHostVisible(unif.buf).BindBuffer(unif.buf);

		unif.desc.buffer(unif.buf);
		unif.desc.offset(0);
		unif.desc.range(size);

		unif.source = ptr;
	}

	void Shader::UploadUniforms()
	{
		for (auto &entry : uniforms)
		{
			auto &unif = entry.second;

			// TODO(pushrax): perhaps hash contents?
			void *target = unif.mem.Map();
			memcpy(target, unif.source, unif.desc.range());
			unif.mem.Unmap();
		}
	}

	vk::PipelineShaderStageCreateInfo Shader::StageCreateInfo()
	{
		vk::PipelineShaderStageCreateInfo stageInfo;
		stageInfo.stage(type->stage);
		stageInfo.module(compileOutput->module);
		stageInfo.pName("main");
		return stageInfo;
	}

	void ShaderCompileOutput::addIdentifier(uint32 id, const char *name)
	{
		identifiers[id] = std::string(name);
	}

	void ShaderCompileOutput::addDescriptorSet(uint32 id, uint32 set)
	{
		auto name = identifiers[id];
		descriptorSets[name] = set;
		Debugf("%s has descriptor set %d", name.c_str(), set);
	}

	void ShaderCompileOutput::addBinding(uint32 id, uint32 binding)
	{
		auto name = identifiers[id];
		bindings[name] = binding;
		Debugf("%s has binding %d", name.c_str(), binding);
	}

	void ShaderCompileOutput::addLocation(uint32 id, uint32 location)
	{
		auto name = identifiers[id];
		locations[name] = location;
		Debugf("%s has location %d", name.c_str(), location);
	}
}

// NEXT
/*
shader manager with static list of ShaderMeta instances
ShaderCompileOutput type with descriptor info etc
eagerly compile shaders, but not shader instances
*/