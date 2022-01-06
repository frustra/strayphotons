#include "ShaderManager.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "core/Hashing.hh"
#include "core/Logging.hh"

#include <fstream>
#include <sstream>

namespace sp {
    vector<ShaderMeta *> &ShaderManager::ShaderTypes() {
        static vector<ShaderMeta *> shaderTypes;
        return shaderTypes;
    }

    void ShaderManager::RegisterShaderType(ShaderMeta *metaType) {
        ShaderTypes().push_back(metaType);
    }

    std::unordered_map<string, string> &ShaderManager::DefineVars() {
        static std::unordered_map<string, string> defineVars;
        return defineVars;
    }

    void ShaderManager::SetDefine(string name, string value) {
        DefineVars()[name] = value;
    }

    void ShaderManager::SetDefine(string name, bool value) {
        if (value) {
            DefineVars()[name] = "1";
        } else {
            DefineVars().erase(name);
        }
    }

    ShaderManager::~ShaderManager() {
        for (auto cached : pipelineCache)
            glDeleteProgramPipelines(1, &cached.second);
    }

    void ShaderManager::CompileAll(ShaderSet &shaders) {
        for (auto shaderType : ShaderTypes()) {
            auto input = LoadShader(shaderType);
            auto output = CompileShader(input);
            if (!output) continue;

            output->shaderType = shaderType;
            auto shader = shaderType->newInstance(output);
            shared_ptr<Shader> shaderPtr(shader);
            shaders.shaders[shaderType] = shaderPtr;
        }
    }

    shared_ptr<ShaderCompileOutput> ShaderManager::CompileShader(ShaderCompileInput &input) {
        // Call glGetError() to ensure we have a clean GL error state
        glGetError();

        auto sourceStr = input.source.c_str();
        GLuint program = glCreateShaderProgramv(input.shaderType->GLStage(), 1, &sourceStr);
        Assert(program, "failed to create shader program");
        AssertGLOK("glCreateShaderProgramv");

        int linked;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);

        if (!linked) {
            int infoLogLength;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);

            char *infoLog = new char[infoLogLength + 1];
            glGetProgramInfoLog(program, infoLogLength + 1, nullptr, infoLog);

            auto err = ProcessError(input, string(infoLog));
            Errorf("%s", err);
#ifdef SP_PACKAGE_RELEASE
            Abort(err);
#else
            return nullptr;
#endif
        }

        auto output = make_shared<ShaderCompileOutput>();
        output->shaderType = input.shaderType;
        output->program = program;
        return output;
    }

    ShaderCompileInput ShaderManager::LoadShader(ShaderMeta *shaderType) {
        auto input = ShaderCompileInput{shaderType};
        input.source = LoadShader(input, shaderType->filename);
        return input;
    }

    string ShaderManager::LoadShader(ShaderCompileInput &input, string name) {
        string filePath = "shaders/" + name;
        auto asset = GAssets.Load(filePath, AssetType::Bundled, true);
        Assert(asset, "Shader asset not found");
        asset->WaitUntilValid();
        input.units.push_back(name);

        string relativePath;
        auto dirIndex = name.rfind('/');
        if (dirIndex != string::npos) relativePath = name.substr(0, dirIndex);
        return ProcessShaderSource(input, asset->String(), relativePath);
    }

    string ShaderManager::ProcessShaderSource(ShaderCompileInput &input, const string &src, const string &path) {
        std::istringstream lines(src);
        string line;
        std::ostringstream output;
        size_t linesProcessed = 0, currUnit = input.units.size() - 1;

        while (std::getline(lines, line)) {
            linesProcessed++;

            if (starts_with(line, "#version")) {
                output << line << std::endl;
                for (auto define : DefineVars()) {
                    output << "#define " << define.first << " " << define.second << std::endl;
                }
                output << "#line " << (linesProcessed + 1) << " " << currUnit << std::endl;
                continue;
            } else if (starts_with(line, "#include ")) {
                trim(line);
                string importPath = path + "/" + line.substr(10, line.length() - 11);
                string importSrc = LoadShader(input, importPath);

                size_t nextUnit = input.units.size();
                output << "//start " << line << std::endl;
                output << "#line 1 " << nextUnit << std::endl;
                output << importSrc << std::endl;
                output << "//end " << line << std::endl;
                output << "#line " << (linesProcessed + 1) << " " << currUnit << std::endl;
                continue;
            } else if (line[0] != '#' || line[1] != '#') {
                output << line << std::endl;
                continue;
            }

            std::istringstream tokens(line.substr(2));
            string command;
            tokens >> command;

            if (command == "import") {
                size_t nextUnit = input.units.size();

                string importPath;
                std::getline(tokens, importPath);
                trim(importPath);
                importPath += ".glsl";

                string importSrc = LoadShader(input, importPath);

                output << "//start " << line << std::endl;
                output << "#line 1 " << nextUnit << std::endl;
                output << importSrc << std::endl;
                output << "//end " << line << std::endl;
                output << "#line " << (linesProcessed + 1) << " " << currUnit << std::endl;
            } else {
                Abortf("invalid shader command %s #%s", command, input.units.back());
            }
        }

        return output.str();
    }

    string ShaderManager::ProcessError(ShaderCompileInput &input, string err) {
        std::istringstream lines(err);
        string line;
        std::ostringstream output;
        bool newline = false;

        while (std::getline(lines, line)) {
            trim(line);

            int lineNumber = -1;
            string unitName = input.units[0];

            vector<string> integers;
            string lastInteger;

            for (size_t i = 0; i <= line.length(); i++) {
                if (i < line.length() && line[i] > 0 && isdigit(line[i])) {
                    lastInteger += line[i];
                } else if (lastInteger.length()) {
                    integers.push_back(lastInteger);
                    lastInteger = "";
                }
            }

            // Assume first two integers are the unit# and line#
            if (integers.size() >= 2) {
                uint32 unit = std::stoul(integers[0]);
                lineNumber = std::stoi(integers[1]);

                if (unit < input.units.size()) { unitName = input.units[unit]; }
            }

            if (newline) output << std::endl;
            newline = true;

            if (input.units[0] == unitName || input.units[0] == "") {
                output << unitName << ":" << lineNumber << " " << line;
            } else {
                output << unitName << ":" << lineNumber << " (via " << input.units[0] << ") " << line;
            }
        }

        return output.str();
    }

    void ShaderManager::BindPipeline(vector<ShaderMeta *> shaderMetaTypes) {
        size_t hash = 0;

        for (auto shaderMeta : shaderMetaTypes) {
            auto shader = shaders.Get(shaderMeta);
            hash_combine(hash, shader->GLProgram());
            shader->BindBuffers();
        }

        auto cached = pipelineCache.find(hash);
        if (cached != pipelineCache.end()) {
            glBindProgramPipeline(cached->second);
            return;
        }

        GLuint pipeline;
        glGenProgramPipelines(1, &pipeline);

        for (auto shaderMeta : shaderMetaTypes) {
            auto shader = shaders.Get(shaderMeta);
            glUseProgramStages(pipeline, shaderMeta->GLStageBits(), shader->GLProgram());
        }

        glBindProgramPipeline(pipeline);
        pipelineCache[hash] = pipeline;
    }
} // namespace sp
