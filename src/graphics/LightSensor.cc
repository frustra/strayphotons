#include "LightSensor.hh"
#include "core/Logging.hh"
#include "core/Console.hh"
#include "ecs/components/VoxelInfo.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/events/SignalChange.hh"

#include <cmath>

namespace sp
{
	LightSensorUpdateCS::LightSensorUpdateCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		BindBuffer(sensorData, 0);
		BindBuffer(lightData, 1);
		BindBuffer(voxelInfo, 2);
		Bind(lightCount, "lightCount");

		readBackSize = sizeof(float) * 4 * MAX_SENSORS * 2;
		outputTex.Create().Size(MAX_SENSORS * 2, 1).Storage(PF_RGBA32F);
		readBackBuf.Create().Data(readBackSize, nullptr, GL_STREAM_READ);
	}

	void LightSensorUpdateCS::SetSensors(ecs::EntityManager::EntityCollection &sensors)
	{
		int N = 0;
		GLLightSensorData data[MAX_SENSORS + 1];

		for (auto entity : sensors)
		{
			auto sensor = entity.Get<ecs::LightSensor>();
			auto transform = entity.Get<ecs::Transform>();
			auto id = entity.GetId();

			GLLightSensorData &s = data[N++];
			auto mat = transform->GetGlobalTransform();
			s.position = mat * glm::vec4(sensor->position, 1);
			s.direction = glm::normalize(glm::mat3(mat) * sensor->direction);
			s.id0 = (float) id.Index();
			s.id1 = (float) id.Generation();
		}

		*(uint32 *)&data[MAX_SENSORS] = N;

		BufferData(sensorData, sizeof(GLLightSensorData) * MAX_SENSORS + sizeof(uint32), &data[0]);
	}

	void LightSensorUpdateCS::SetLightData(int count, GLLightData *data)
	{
		Set(lightCount, count);
		BufferData(lightData, sizeof(GLLightData) * count, data);
	}

	void LightSensorUpdateCS::SetVoxelInfo(GLVoxelInfo *data)
	{
		BufferData(voxelInfo, sizeof(GLVoxelInfo), data);
	}

	void LightSensorUpdateCS::StartReadback()
	{
		readBackBuf.Bind(GL_PIXEL_PACK_BUFFER);
		glGetTextureImage(outputTex.handle, 0, GL_RGBA, GL_FLOAT, readBackSize, 0);
	}

	void LightSensorUpdateCS::UpdateValues(ecs::EntityManager &manager)
	{
		float *buf = (float *) readBackBuf.Map(GL_READ_ONLY);
		if (buf == nullptr)
		{
			Errorf("Missed readback of light sensor buffer");
			return;
		}

		while (buf[0] == 1.0f)
		{
			ecs::Entity::Id eid((ecs::eid_t) buf[1], (ecs::gen_t) buf[2]);

			buf += 4;
			glm::vec3 lum(buf[0], buf[1], buf[2]);
			buf += 4;

			ecs::Entity sensorEnt(&manager, eid);
			if (sensorEnt.Valid())
			{
				auto sensor = sensorEnt.Get<ecs::LightSensor>();
				auto triggers = sensor->triggers;
				auto prev = sensor->illuminance;
				sensor->illuminance = lum;

				glm::vec3 triggerLevels(0, 0, 0);

				for (auto trigger : triggers)
				{
					for (size_t i = 0; i < 3; ++i)
					{
						triggerLevels[i] += std::min(1.0f, lum[i] / trigger.illuminance[i]);
					}

					if (trigger(lum) && !trigger(prev))
					{
						ecs::SignalChange sig(trigger.onSignal);
						sensorEnt.Emit(sig);
						GConsoleManager.ParseAndExecute(trigger.oncmd);
					}
					if (!trigger(lum) && trigger(prev))
					{
						ecs::SignalChange sig(trigger.offSignal);
						sensorEnt.Emit(sig);
						GConsoleManager.ParseAndExecute(trigger.offcmd);
					}
				}

				// scale the emissiveness r & g colours by how close
				// the sensor is to being fully activated
				if (sensorEnt.Has<ecs::Renderable>())
				{
					triggerLevels /= 3*triggers.size();
					float triggerLevel = triggerLevels.x + triggerLevels.y + triggerLevels.z;

					// triggerLevel in [0, 1]
					float boost = 0.1;
					if (triggerLevel < 1.0 - 5 * std::numeric_limits<float>::epsilon())
					{
						boost *= -1;
					}

					float g = std::max(0.0f, (1 - boost) * triggerLevel + boost);

					auto renderable = sensorEnt.Get<ecs::Renderable>();
					renderable->emissive.g = g;
					renderable->emissive.r = 1 - g;
				}
			}
		}
		readBackBuf.Unmap();
	}

	IMPLEMENT_SHADER_TYPE(LightSensorUpdateCS, "light_sensor_update.comp", Compute);
}