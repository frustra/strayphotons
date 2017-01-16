#include "graphics/ShaderManager.hh"
#include "core/Logging.hh"

#include <fstream>
#include <sstream>
#include <boost/functional/hash.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

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

	void ShaderManager::CompileAll(ShaderSet *shaders)
	{
		for (auto shaderType : ShaderTypes())
		{
			auto input = LoadShader(shaderType);
			auto output = CompileShader(input);
			if (!output) continue;

			output->shaderType = shaderType;
			auto shader = shaderType->newInstance(output);
			shared_ptr<Shader> shaderPtr(shader);
			shaders->shaders[shaderType] = shaderPtr;
		}
	}

	shared_ptr<ShaderCompileOutput> ShaderManager::CompileShader(ShaderCompileInput &input)
	{
		auto sourceStr = input.source.c_str();
		GLuint program = glCreateShaderProgramv(input.shaderType->GLStage(), 1, &sourceStr);
		Assert(program, "failed to create shader program");
		AssertGLOK("glCreateShaderProgramv");

		int linked;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);

		if (!linked)
		{
			int infoLogLength;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);

			char *infoLog = new char[infoLogLength + 1];
			glGetProgramInfoLog(program, infoLogLength + 1, nullptr, infoLog);

			auto err = ProcessError(input, string(infoLog));
			Errorf("%s", err);
			throw std::runtime_error(err);
		}

		auto output = make_shared<ShaderCompileOutput>();
		output->shaderType = input.shaderType;
		output->program = program;
		return output;
	}

	// TODO(pushrax): use asset manager
	string loadFile(string path)
	{
		string filename = "../src/shaders/" + path;

		std::ifstream fin(filename, std::ios::binary);
		if (!fin.good())
		{
			Errorf("Shader file %s could not be read", path);
			throw std::runtime_error("missing shader: " + path);
		}

		return string((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
	}

	string formatError(string err, string unit, string root, int line)
	{
		if (root == unit || root == "")
			return boost::str(boost::format("%s:%d %s") % unit % line % err);
		else
			return boost::str(boost::format("%s:%d (via %s) %s") % unit % line % root % err);
	}

	ShaderCompileInput ShaderManager::LoadShader(ShaderMeta *shaderType)
	{
		auto input = ShaderCompileInput {shaderType};
		input.source = LoadShader(input, shaderType->filename);
		return input;
	}

	string ShaderManager::LoadShader(ShaderCompileInput &input, string name)
	{
		string src = loadFile(name);
		input.units.push_back(name);
		return ProcessShaderSource(input, src);
	}

	string ShaderManager::ProcessShaderSource(ShaderCompileInput &input, string src)
	{
		std::istringstream lines(src);
		string line;
		vector<string> output;
		int linesProcessed = 0, currUnit = input.units.size() - 1;

		while (std::getline(lines, line))
		{
			linesProcessed++;

			if (boost::starts_with(line, "#version"))
			{
				output.push_back(line);
				string vendorStr = (char *) glGetString(GL_VENDOR);
				if (boost::starts_with(vendorStr, "NVIDIA"))
				{
					output.push_back("#define NVIDIA_GPU");
				}
				else if (boost::starts_with(vendorStr, "AMD"))
				{
					output.push_back("#define AMD_GPU");
				}
				else if (boost::starts_with(vendorStr, "Intel"))
				{
					output.push_back("#define INTEL_GPU");
				}
				else
				{
					output.push_back("#define UNKNOWN_GPU");
				}
				output.push_back(boost::str(boost::format("#line %d %d") % (linesProcessed + 1) % currUnit));
				continue;
			}
			else if (line[0] != '#' || line[1] != '#')
			{
				output.push_back(line);
				continue;
			}

			std::istringstream tokens(line.substr(2));
			string command;
			tokens >> command;

			if (command == "import")
			{
				int nextUnit = input.units.size();

				string importPath;
				std::getline(tokens, importPath);
				boost::algorithm::trim(importPath);
				importPath += ".glsl";

				string importSrc = LoadShader(input, importPath);

				output.push_back("//start " + line);
				output.push_back(boost::str(boost::format("#line 1 %d") % nextUnit));
				output.push_back(importSrc);
				output.push_back("//end " + line);
				output.push_back(boost::str(boost::format("#line %d %d") % (linesProcessed + 1) % currUnit));
			}
			else
			{
				throw runtime_error("invalid shader command " + command + " #" + input.units.back());
			}
		}

		return boost::algorithm::join(output, "\n");
	}

	string ShaderManager::ProcessError(ShaderCompileInput &input, string err)
	{
		std::istringstream lines(err);
		string line;
		vector<string> output;

		while (std::getline(lines, line))
		{
			boost::trim(line);

			int lineNumber = -1;
			string unitName = input.units[0];

			vector<string> integers;
			string lastInteger;

			for (size_t i = 0; i <= line.length(); i++)
			{
				if (i < line.length() && line[i] > 0 && isdigit(line[i]))
				{
					lastInteger += line[i];
				}
				else if (lastInteger.length())
				{
					integers.push_back(lastInteger);
					lastInteger = "";
				}
			}

			// Assume first two integers are the unit# and line#
			if (integers.size() >= 2)
			{
				uint32 unit = std::stoul(integers[0]);
				lineNumber = std::stoi(integers[1]);

				if (unit < input.units.size())
				{
					unitName = input.units[unit];
				}
			}

			output.push_back(formatError(line, unitName, input.units[0], lineNumber));
		}

		return boost::algorithm::join(output, "\n");
	}

	void ShaderManager::BindPipeline(ShaderSet *shaders, vector<ShaderMeta *> shaderMetaTypes)
	{
		size_t hash = 0;

		for (auto shaderMeta : shaderMetaTypes)
		{
			auto shader = shaders->Get(shaderMeta);
			boost::hash_combine(hash, shader->GLProgram());
			shader->BindBuffers();
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
			auto shader = shaders->Get(shaderMeta);
			glUseProgramStages(pipeline, shaderMeta->GLStageBits(), shader->GLProgram());
		}

		glBindProgramPipeline(pipeline);
		pipelineCache[hash] = pipeline;
	}
}
