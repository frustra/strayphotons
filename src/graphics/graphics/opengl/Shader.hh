#pragma once

#include "core/Common.hh"
#include "graphics/opengl/Graphics.hh"

#include <robin_hood.h>

#define SHADER_TYPE(ClassName)                                                                                         \
public:                                                                                                                \
    static ShaderMeta MetaType;                                                                                        \
    static Shader *NewInstance(shared_ptr<ShaderCompileOutput> compileOutput) {                                        \
        return new ClassName(compileOutput);                                                                           \
    }

#define IMPLEMENT_SHADER_TYPE(cls, file, stage)                                                                        \
    ShaderMeta cls::MetaType {                                                                                         \
        #cls, file, ShaderStage::stage, cls::NewInstance                                                               \
    }

namespace sp {
    class Shader;
    class ShaderCompileOutput;

    enum class ShaderStage {
        Vertex = GL_VERTEX_SHADER,
        Geometry = GL_GEOMETRY_SHADER,
        Fragment = GL_FRAGMENT_SHADER,
        Compute = GL_COMPUTE_SHADER,
        TessControl = GL_TESS_CONTROL_SHADER,
        TessEval = GL_TESS_EVALUATION_SHADER,
    };

    class ShaderMeta {
    public:
        typedef Shader *(*Constructor)(shared_ptr<ShaderCompileOutput>);

        ShaderMeta(string name, string filename, ShaderStage stage, Constructor newInstance);

        const string name;
        const string filename;
        const ShaderStage stage;

        const GLbitfield GLStageBits() {
            switch (stage) {
            case ShaderStage::Vertex:
                return GL_VERTEX_SHADER_BIT;
            case ShaderStage::Geometry:
                return GL_GEOMETRY_SHADER_BIT;
            case ShaderStage::Fragment:
                return GL_FRAGMENT_SHADER_BIT;
            case ShaderStage::Compute:
                return GL_COMPUTE_SHADER_BIT;
            case ShaderStage::TessControl:
                return GL_TESS_CONTROL_SHADER_BIT;
            case ShaderStage::TessEval:
                return GL_TESS_EVALUATION_SHADER_BIT;
            default:
                throw std::runtime_error("Unknown shader stage bit");
            }
        }

        const GLenum GLStage() {
            return (GLenum)stage;
        }

        Constructor newInstance;
    };

    class ShaderCompileInput {
    public:
        ShaderMeta *shaderType;
        string source;
        vector<string> units;
    };

    class ShaderCompileOutput {
    public:
        ShaderMeta *shaderType;
        GLuint program;

    private:
    };

    class Shader {
    public:
        Shader(shared_ptr<ShaderCompileOutput> compileOutput);
        virtual ~Shader();

        struct ShaderBuffer {
            GLint index = -1;
            size_t size;
            GLuint handle = 0;
            GLenum target, usage;
        };

        typedef ShaderBuffer UniformBuffer;
        typedef ShaderBuffer StorageBuffer;

        struct Uniform {
            string name;
            GLint location = -1;
        };

        vector<ShaderBuffer *> buffers;

        const GLuint GLProgram() {
            return compileOutput->program;
        }

        void BindBuffers();

    protected:
        Uniform &LookupUniform(string name);
        void BindBuffer(ShaderBuffer &b, int index, GLenum target = GL_UNIFORM_BUFFER, GLenum usage = GL_STATIC_DRAW);
        void BufferData(ShaderBuffer &b, GLsizei size, const void *data);

        bool IsBound(Uniform &u) {
            return u.location != -1;
        };
        bool IsBound(ShaderBuffer &b) {
            return b.index != -1;
        };

        // Methods for setting uniform values.

        template<typename ValueType>
        void Set(string name, const ValueType &v) {
            Set(LookupUniform(name), v);
        }

        template<typename ValueType>
        void Set(string name, const ValueType v[], GLsizei count) {
            Set(LookupUniform(name), v, count);
        }

        template<typename ValueType>
        void Set(Uniform &u, const ValueType &v) {
            Assert(false, "unimplemented uniform type");
        }

        template<typename ValueType>
        void Set(Uniform &u, const ValueType v[], GLsizei count) {
            Assert(false, "unimplemented uniform type");
        }

    private:
        ShaderMeta *type;
        GLuint program;
        shared_ptr<ShaderCompileOutput> compileOutput;
        robin_hood::unordered_flat_map<string, Uniform> uniforms;
    };

    class ShaderSet {
    public:
        typedef robin_hood::unordered_flat_map<ShaderMeta *, shared_ptr<Shader>> MapType;

        const MapType &Map() const {
            return shaders;
        }

        shared_ptr<Shader> Get(ShaderMeta *meta) const {
            auto ptr = shaders.find(meta);

            if (ptr == shaders.end()) throw std::runtime_error("shader not loaded: " + meta->name);

            return ptr->second;
        }

        template<typename ShaderType>
        ShaderType *Get() const {
            return static_cast<ShaderType *>(Get(&ShaderType::MetaType).get());
        }

    private:
        MapType shaders;

        friend class ShaderManager;
    };

    template<>
    inline void Shader::Set<bool>(Uniform &u, const bool &v) {
        glProgramUniform1i(program, u.location, v);
    }

#define DECLARE_SET_SCALAR(UniformSuffix, ValueType, GLType)                                                           \
    template<>                                                                                                         \
    inline void Shader::Set<ValueType>(Uniform & u, const ValueType &v) {                                              \
        glProgramUniform##UniformSuffix(program, u.location, 1, &v);                                                   \
    }                                                                                                                  \
    template<>                                                                                                         \
    inline void Shader::Set<ValueType>(Uniform & u, const ValueType v[], GLsizei count) {                              \
        glProgramUniform##UniformSuffix(program, u.location, count, (const GLType *)v);                                \
    }

#define DECLARE_SET_GLM(UniformSuffix, ValueType, GLType)                                                              \
    template<>                                                                                                         \
    inline void Shader::Set<ValueType>(Uniform & u, const ValueType &v) {                                              \
        glProgramUniform##UniformSuffix(program, u.location, 1, glm::value_ptr(v));                                    \
    }                                                                                                                  \
    template<>                                                                                                         \
    inline void Shader::Set<ValueType>(Uniform & u, const ValueType v[], GLsizei count) {                              \
        glProgramUniform##UniformSuffix(program, u.location, count, (const GLType *)v);                                \
    }

#define DECLARE_SET_GLM_MAT(UniformSuffix, ValueType, GLType)                                                          \
    template<>                                                                                                         \
    inline void Shader::Set<ValueType>(Uniform & u, const ValueType &v) {                                              \
        glProgramUniformMatrix##UniformSuffix(program, u.location, 1, 0, glm::value_ptr(v));                           \
    }                                                                                                                  \
    template<>                                                                                                         \
    inline void Shader::Set<ValueType>(Uniform & u, const ValueType v[], GLsizei count) {                              \
        glProgramUniformMatrix##UniformSuffix(program, u.location, count, 0, (const GLType *)v);                       \
    }

    DECLARE_SET_SCALAR(1fv, float, GLfloat)
    DECLARE_SET_SCALAR(1iv, int32, GLint)
    DECLARE_SET_SCALAR(1uiv, uint32, GLuint)
    DECLARE_SET_GLM(2fv, glm::vec2, GLfloat)
    DECLARE_SET_GLM(3fv, glm::vec3, GLfloat)
    DECLARE_SET_GLM(4fv, glm::vec4, GLfloat)
    DECLARE_SET_GLM(2iv, glm::i32vec2, GLint)
    DECLARE_SET_GLM(3iv, glm::i32vec3, GLint)
    DECLARE_SET_GLM(4iv, glm::i32vec4, GLint)
    DECLARE_SET_GLM(2uiv, glm::u32vec2, GLuint)
    DECLARE_SET_GLM(3uiv, glm::u32vec3, GLuint)
    DECLARE_SET_GLM(4uiv, glm::u32vec4, GLuint)
    DECLARE_SET_GLM_MAT(2fv, glm::mat2, GLfloat)
    DECLARE_SET_GLM_MAT(3fv, glm::mat3, GLfloat)
    DECLARE_SET_GLM_MAT(4fv, glm::mat4, GLfloat)

#undef DECLARE_SET_SCALAR
#undef DECLARE_SET_GLM
#undef DECLARE_SET_GLM_MAT
} // namespace sp
