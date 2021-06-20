#include "LightSensor.hh"

#include "core/Console.hh"
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

#pragma pack(push, 1)
    struct GLSensorDataBuffer {
        uint32_t sensorCount = 0;
        GLLightSensorData sensors[LightSensorUpdateCS::MAX_SENSORS];
    };
#pragma pack(pop)

    void LightSensorUpdateCS::SetSensors(std::vector<ecs::Entity> &sensors) {
        GLSensorDataBuffer data = { 0 };

        for (auto entity : sensors) {
            auto sensor = entity.Get<ecs::LightSensor>();
            auto id = entity.GetId();

            GLLightSensorData &s = data.sensors[data.sensorCount++];
            glm::mat4 mat;
            {
                auto lock = entity.GetManager()->tecs.StartTransaction<ecs::Read<ecs::Transform>>();
                auto &transform = entity.GetEntity().Get<ecs::Transform>(lock);
                mat = transform.GetGlobalTransform(lock);
            }
            s.position = mat * glm::vec4(sensor->position, 1);
            s.direction = glm::normalize(glm::mat3(mat) * sensor->direction);
            // TODO: Fix this so it doesn't lose precision
            s.id0 = (float)id; //.Index();
            // s.id1 = (float)id.Generation();
        }

        BufferData(sensorData, sizeof(GLSensorDataBuffer), &data);
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

    void LightSensorUpdateCS::UpdateValues(ecs::EntityManager &manager) {
        float *buf = (float *)readBackBuf.Map(GL_READ_ONLY);
        if (buf == nullptr) {
            Errorf("Missed readback of light sensor buffer");
            return;
        }

        while (buf[0] == 1.0f) {
            // TODO: Fix this to read full precision
            // ecs::Entity::Id eid((ecs::eid_t)buf[1], (ecs::gen_t)buf[2]);
            ecs::Entity::Id eid((size_t)buf[1]);

            buf += 4;
            glm::vec3 lum(buf[0], buf[1], buf[2]);
            buf += 4;

            ecs::Entity sensorEnt(&manager, eid);
            if (sensorEnt.Valid() && sensorEnt.Has<ecs::LightSensor>()) {
                auto sensor = sensorEnt.Get<ecs::LightSensor>();
                sensor->illuminance = lum;
            }
        }
        readBackBuf.Unmap();
    }

    IMPLEMENT_SHADER_TYPE(LightSensorUpdateCS, "light_sensor_update.comp", Compute);
} // namespace sp
