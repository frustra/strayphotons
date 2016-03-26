#include "graphics/ShaderManager.hh"
#include "core/Logging.hh"

#include <fstream>
#include <spirv/spirv.h>
#include <spirv-tools/libspirv.h>

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
	}

	void ShaderManager::CompileAll(ShaderSet &shaders)
	{
		for (auto shaderType : ShaderTypes())
		{
			auto input = LoadShader(shaderType);
			auto output = CompileShader(input);
			output->device = &device;
			output->shaderType = shaderType;
			auto shader = shaderType->newInstance(output);
			shared_ptr<Shader> shaderPtr(shader);
			shaders.shaders[shaderType] = shaderPtr;
		}
	}

	shared_ptr<ShaderCompileOutput> ShaderManager::CompileShader(ShaderCompileInput &input)
	{

		vk::ShaderModuleCreateInfo moduleInfo;
		moduleInfo.codeSize(input.source.size());
		moduleInfo.pCode(reinterpret_cast<const uint32 *>(input.source.data()));

		auto output = make_shared<ShaderCompileOutput>();
		output->module = device->createShaderModule(moduleInfo, nullptr);

		return output;
	}

	ShaderCompileInput ShaderManager::LoadShader(ShaderMeta *shaderType)
	{
		string filename = "./assets/shaders/" + shaderType->filename + ".spv";

		std::ifstream fin(filename, std::ios::binary);
		if (!fin.good())
		{
			Errorf("Shader file %s could not be read", filename.c_str());
			throw std::runtime_error("missing shader: " + shaderType->filename);
		}

		vector<char> buffer((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
		return ShaderCompileInput{shaderType, buffer};
	}

	static spv_result_t visitHeader(void *userData, spv_endianness_t endian, uint32, uint32 version, uint32 gen, uint32 id, uint32 schema)
	{
		return SPV_SUCCESS;
	}

	static spv_result_t visitInstruction(void *userData, const spv_parsed_instruction_t *inst)
	{
		auto output = static_cast<ShaderCompileOutput *>(userData);
		auto num_operands = inst->num_operands;
		auto operands = inst->operands;

		switch (inst->opcode)
		{
			case SpvOpDecorate:
			{
				Assert(num_operands >= 2);
				Assert(operands[0].type == SPV_OPERAND_TYPE_ID);
				Assert(operands[1].type == SPV_OPERAND_TYPE_DECORATION);

				auto id = inst->words[operands[0].offset];
				auto decoration = inst->words[operands[1].offset];

				if (decoration == SpvDecorationDescriptorSet)
				{
					Assert(num_operands == 3);
					Assert(operands[2].type == SPV_OPERAND_TYPE_LITERAL_INTEGER);
					auto set = inst->words[operands[2].offset];
					output->addDescriptorSet(id, set);
				}
				else if (decoration == SpvDecorationBinding)
				{
					Assert(num_operands == 3);
					Assert(operands[2].type == SPV_OPERAND_TYPE_LITERAL_INTEGER);
					auto binding = inst->words[operands[2].offset];
					output->addBinding(id, binding);
				}
				else if (decoration == SpvDecorationLocation)
				{
					Assert(num_operands == 3);
					Assert(operands[2].type == SPV_OPERAND_TYPE_LITERAL_INTEGER);
					auto location = inst->words[operands[2].offset];
					output->addLocation(id, location);
				}
				break;
			}

			case SpvOpName:
			{
				Assert(num_operands == 2);
				Assert(operands[0].type == SPV_OPERAND_TYPE_ID);
				Assert(operands[1].type == SPV_OPERAND_TYPE_LITERAL_STRING);

				auto id = inst->words[operands[0].offset];
				auto name = inst->words + operands[1].offset;
				output->addIdentifier(id, reinterpret_cast<const char *>(name));
				break;
			}
		}
		return SPV_SUCCESS;
	}

	void ShaderManager::ParseShader(ShaderCompileInput &input, ShaderCompileOutput *output)
	{
		spv_context ctx = spvContextCreate();
		spv_diagnostic diagnostic = nullptr;
		const uint32 *binary = reinterpret_cast<const uint32 *>(input.source.data());
		auto err = spvBinaryParse(ctx, &output, binary, input.source.size() / sizeof(*binary), visitHeader, visitInstruction, &diagnostic);
		spvContextDestroy(ctx);

		if (err)
		{
			spvDiagnosticPrint(diagnostic);
			spvDiagnosticDestroy(diagnostic);
			throw std::runtime_error("error parsing spirv for " + input.shaderType->filename);
		}
	}
}