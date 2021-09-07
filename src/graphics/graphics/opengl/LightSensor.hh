#pragma once

#include "ecs/Ecs.hh"
#include "graphics/opengl/GLBuffer.hh"
#include "graphics/opengl/GLTexture.hh"
#include "graphics/opengl/GPUTypes.hh"
#include "graphics/opengl/Shader.hh"
#include "graphics/opengl/ShaderManager.hh"

#include <vector>

namespace sp {
    class LightSensorUpdateCS : public Shader {
        SHADER_TYPE(LightSensorUpdateCS);
        LightSensorUpdateCS(shared_ptr<ShaderCompileOutput> compileOutput);

        void SetSensors(ecs::Lock<ecs::Read<ecs::LightSensor, ecs::Transform>> lock);
        void SetLightData(int count, GLLightData *data);
        void SetVoxelInfo(GLVoxelInfo *data);
        void StartReadback();
        void UpdateValues(ecs::Lock<ecs::Write<ecs::LightSensor>> lock);

        static const int MAX_SENSORS = 32;

        GLTexture outputTex;

    private:
        UniformBuffer sensorData, lightData, voxelInfo;
        GLBuffer readBackBuf;
        GLsizei readBackSize;
    };
} // namespace sp
