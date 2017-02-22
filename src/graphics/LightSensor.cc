#include "LightSensor.hh"
#include "core/Logging.hh"

namespace sp
{
	LightSensorUpdateCS::LightSensorUpdateCS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
	{
		BindBuffer(sensorBuf, 0);

		readBackSize = sizeof(float) * 4 * MAX_SENSORS * 2;
		outputTex.Create().Size(MAX_SENSORS * 2, 1).Storage(PF_RGBA32F);
		readBackBuf.Create().Data(readBackSize, nullptr, GL_STREAM_READ);
	}

	void LightSensorUpdateCS::SetSensors(ecs::EntityManager::EntityCollection &sensors)
	{
		int N = 0;
		GLLightSensorData sensorData[MAX_SENSORS + 1];

		for (auto entity : sensors)
		{
			auto sensor = entity.Get<ecs::LightSensor>();
			auto transform = entity.Get<ecs::Transform>();
			auto id = entity.GetId();

			GLLightSensorData &s = sensorData[N++];
			auto mat = transform->GetModelTransform(*entity.GetManager());
			s.position = mat * glm::vec4(0, 0, 0, 1);
			s.direction = glm::normalize(glm::mat3(mat) * glm::vec3(0, 0, -1));
			s.id0 = ((float *)&id)[0];
			s.id1 = ((float *)&id)[1];
		}

		*(uint32 *)&sensorData[MAX_SENSORS] = N;

		BufferData(sensorBuf, sizeof(GLLightSensorData) * MAX_SENSORS + sizeof(uint32), &sensorData[0]);
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
			ecs::Entity::Id eid;
			static_assert(sizeof(eid) == sizeof(float) * 2, "Entity::Id has wrong size");

			((float *)&eid)[0] = buf[1];
			((float *)&eid)[1] = buf[2];

			buf += 4;
			glm::vec3 lum(buf[0], buf[1], buf[2]);
			buf += 4;

			manager.Get<ecs::LightSensor>(eid)->illuminance = lum;
		}
		readBackBuf.Unmap();
	}

	IMPLEMENT_SHADER_TYPE(LightSensorUpdateCS, "light_sensor_update.comp", Compute);
}