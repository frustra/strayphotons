/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "LightSensors.hh"

#include "ecs/EcsImpl.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Readback.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

#include <algorithm>

namespace sp::vulkan::renderer {
    const size_t MAX_LIGHT_SENSORS = 64;

    struct LightSensorData {
        struct {
            int sensorCount = 0;
            float _padding0[3];

            struct GPUSensor {
                glm::vec3 position;
                float _padding0[1];
                glm::vec3 direction;
                float _padding1[1];
            } sensors[MAX_LIGHT_SENSORS];
        } gpu;

        ecs::Entity entities[MAX_LIGHT_SENSORS];
    };

    void AddLightSensors(RenderGraph &graph,
        GPUScene &scene,
        ecs::Lock<ecs::Read<ecs::LightSensor, ecs::TransformSnapshot>> lock) {
        ZoneScoped;

        shared_ptr<LightSensorData> data;

        for (auto &entity : lock.EntitiesWith<ecs::LightSensor>()) {
            if (!entity.Has<ecs::LightSensor, ecs::TransformSnapshot>(lock)) continue;

            auto &sensor = entity.Get<ecs::LightSensor>(lock);
            auto &transform = entity.Get<ecs::TransformSnapshot>(lock).globalPose;

            if (!data) data = make_shared<LightSensorData>();

            auto &s = data->gpu.sensors[data->gpu.sensorCount];
            s.position = transform * glm::vec4(sensor.position, 1);
            s.direction = glm::normalize(transform.GetRotation() * sensor.direction);

            data->entities[data->gpu.sensorCount] = entity;
            data->gpu.sensorCount++;
        }

        if (!data) return;

        graph.AddPass("LightSensors")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read("ShadowMap.Linear", Access::ComputeShaderSampleImage);
                builder.Read("LightState", Access::ComputeShaderReadUniform);
                builder.CreateBuffer("LightSensorValues",
                    {sizeof(glm::vec4), MAX_LIGHT_SENSORS},
                    Residency::GPU_ONLY,
                    Access::ComputeShaderWrite);
            })
            .Execute([data, textureSet = &scene.textures](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetComputeShader("light_sensor.comp");

                cmd.SetImageView("shadowMap", "ShadowMap.Linear");
                cmd.SetStorageBuffer("LightSensorResults", "LightSensorValues");
                cmd.SetUniformBuffer("LightData", "LightState");
                cmd.UploadUniformData("LightSensorData", &data->gpu);
                cmd.SetBindlessDescriptors(1, textureSet->GetDescriptorSet());

                cmd.Dispatch(1, 1, 1);
            });

        AddBufferReadback(graph, "LightSensorValues", 0, {}, [data](BufferPtr buffer) {
            auto illuminanceValues = (glm::vec4 *)buffer->Mapped();
            auto lock = ecs::StartTransaction<ecs::Write<ecs::LightSensor>>();
            for (int i = 0; i < data->gpu.sensorCount; i++) {
                auto entity = data->entities[i];
                if (entity.Exists(lock)) {
                    auto &sensor = entity.Get<ecs::LightSensor>(lock);
                    sensor.illuminance = illuminanceValues[i];
                }
            }
        });
    }
} // namespace sp::vulkan::renderer
