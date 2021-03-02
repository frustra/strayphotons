#pragma once

#include "ecs/components/LightSensor.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/VoxelInfo.hh"
#include "graphics/Buffer.hh"
#include "graphics/GPUTypes.hh"
#include "graphics/Shader.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/Texture.hh"

#include <ecs/Ecs.hh>
#include <vector>

namespace sp {
    class LightSensorUpdateCS : public Shader {
        SHADER_TYPE(LightSensorUpdateCS);
        LightSensorUpdateCS(shared_ptr<ShaderCompileOutput> compileOutput);

        void SetSensors(std::vector<ecs::Entity> &sensors);
        void SetLightData(int count, GLLightData *data);
        void SetVoxelInfo(GLVoxelInfo *data);
        void StartReadback();
        void UpdateValues(ecs::EntityManager &manager);

        static const int MAX_SENSORS = 32;

        GLTexture outputTex;

    private:
        UniformBuffer sensorData, lightData, voxelInfo;
        Buffer readBackBuf;
        size_t readBackSize;
    };
} // namespace sp
