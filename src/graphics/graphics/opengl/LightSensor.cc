#include "LightSensor.hh"

#include "core/Logging.hh"

#include <cmath>
#include <ecs/EcsImpl.hh>
#include <vector>

namespace sp {
    LightSensorUpdateCS::LightSensorUpdateCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput) {
        BindBuffer(sensorData, 0);
        BindBuffer(lightData, 1);
        BindBuffer(voxelInfo, 2);

        readBackSize = sizeof(float) * 4 * MAX_SENSORS * 2;
        outputTex.Create().Size(MAX_SENSORS * 2, 1).Storage(PF_RGBA32F);
        readBackBuf.Create().Data(readBackSize, nullptr, GL_STREAM_READ);
    }

    void LightSensorUpdateCS::SetSensors(ecs::Lock<ecs::Read<ecs::LightSensor, ecs::Transform>> lock) {
        int count = 0;
        GLLightSensorData data[MAX_SENSORS];

        for (auto &entity : lock.EntitiesWith<ecs::LightSensor>()) {
            if (!entity.Has<ecs::LightSensor, ecs::Transform>(lock)) continue;

            auto &sensor = entity.Get<ecs::LightSensor>(lock);
            auto &transform = entity.Get<ecs::Transform>(lock);

            GLLightSensorData &s = data[count++];
            glm::mat4 mat = transform.GetGlobalTransform(lock);
            s.position = mat * glm::vec4(sensor.position, 1);
            s.direction = glm::normalize(glm::mat3(mat) * sensor.direction);
            // TODO: Fix this so it doesn't lose precision
            s.id0 = (float)entity.id; //.Index();
            // s.id1 = (float)id.Generation();
        }

        Set("sensorCount", count);
        BufferData(sensorData, sizeof(GLLightSensorData) * count, &data[0]);
    }

    void LightSensorUpdateCS::SetLightData(int count, GLLightData *data) {
        Set("lightCount", count);
        BufferData(lightData, sizeof(GLLightData) * count, data);
    }

    void LightSensorUpdateCS::SetVoxelInfo(GLVoxelInfo *data) {
        BufferData(voxelInfo, sizeof(GLVoxelInfo), data);
    }

    void LightSensorUpdateCS::StartReadback() {
        readBackBuf.Bind(GL_PIXEL_PACK_BUFFER);
        glGetTextureImage(outputTex.handle, 0, GL_RGBA, GL_FLOAT, readBackSize, 0);
    }

    void LightSensorUpdateCS::UpdateValues(ecs::Lock<ecs::Write<ecs::LightSensor>> lock) {
        float *buf = (float *)readBackBuf.Map(GL_READ_ONLY);
        if (buf == nullptr) {
            Errorf("Missed readback of light sensor buffer");
            return;
        }

        while (buf[0] == 1.0f) {
            // TODO: Fix this to read full precision
            // ecs::Entity::Id eid((ecs::eid_t)buf[1], (ecs::gen_t)buf[2]);
            Tecs::Entity entity((size_t)buf[1]);

            buf += 4;
            glm::vec3 lum(buf[0], buf[1], buf[2]);
            buf += 4;

            if (entity && entity.Has<ecs::LightSensor>(lock)) {
                auto &sensor = entity.Get<ecs::LightSensor>(lock);
                sensor.illuminance = lum;
            }
        }
        readBackBuf.Unmap();
    }

    IMPLEMENT_SHADER_TYPE(LightSensorUpdateCS, "light_sensor_update.comp", Compute);
} // namespace sp
