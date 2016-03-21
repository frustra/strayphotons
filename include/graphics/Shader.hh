#ifndef SP_SHADER_H
#define SP_SHADER_H

#include "Shared.hh"
#include "graphics/DeviceAllocator.hh"
#include "graphics/Device.hh"
#include <vulkan/vk_cpp.h>
#include <unordered_map>

#define SHADER_TYPE(ClassName) \
	public: \
	static ShaderMeta MetaType; \
	static Shader *NewInstance(shared_ptr<ShaderCompileOutput> compileOutput) \
	{ return new ClassName(compileOutput); }

#define IMPLEMENT_SHADER_TYPE(cls, file, stage) \
	ShaderMeta cls::MetaType{#cls, file, ShaderStage::e##stage, cls::NewInstance}

namespace sp
{
	class Shader;
	class ShaderCompileOutput;

	typedef vk::ShaderStageFlagBits ShaderStage;

	class ShaderMeta
	{
	public:
		typedef Shader *(*Constructor)(shared_ptr<ShaderCompileOutput>);

		ShaderMeta(string name, string filename, ShaderStage stage, Constructor newInstance);

		const string name;
		const string filename;
		const ShaderStage stage;

		Constructor newInstance;
	};

	class ShaderCompileInput
	{
	public:
		ShaderMeta *shaderType;
		vector<char> source;
	};

	class ShaderCompileOutput
	{
	public:
		vk::ShaderModule module;
		Device *device;
		ShaderMeta *shaderType;

	private:
		std::unordered_map<uint32, string> identifiers;
		std::unordered_map<string, uint32> descriptorSets, bindings, locations;

	public:
		void addIdentifier(uint32 id, const char *name);
		void addDescriptorSet(uint32 id, uint32 set);
		void addBinding(uint32 id, uint32 binding);
		void addLocation(uint32 id, uint32 location);
	};

	struct UniformData
	{
		vk::Buffer buf;
		DeviceAllocation mem;
		vk::DescriptorBufferInfo desc;
		void *source;
	};

	class Shader
	{
	public:
		Shader(shared_ptr<ShaderCompileOutput> compileOutput);
		virtual ~Shader();

		void UploadUniforms();
		vk::PipelineShaderStageCreateInfo StageCreateInfo();

		std::unordered_map<string, UniformData> uniforms;
	protected:
		void Bind(void *ptr, size_t size, string name);

		template <typename BufferType>
		void Bind(BufferType &ref, string name)
		{
			Bind(&ref, sizeof(BufferType), name);
		}

	private:
		ShaderMeta *type;
		shared_ptr<ShaderCompileOutput> compileOutput;
		Device &device;
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
}

#endif