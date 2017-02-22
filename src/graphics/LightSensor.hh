#pragma once

#include "graphics/Buffer.hh"
#include "graphics/Texture.hh"
#include "graphics/Shader.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GPUTypes.hh"
#include "ecs/components/LightSensor.hh"
#include "ecs/components/Transform.hh"
#include <Ecs.hh>

namespace sp
{
	class LightSensorUpdateCS : public Shader
	{
		SHADER_TYPE(LightSensorUpdateCS);
		LightSensorUpdateCS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetSensors(ecs::EntityManager::EntityCollection &sensors);
		void StartReadback();
		void UpdateValues(ecs::EntityManager &manager);

		static const int MAX_SENSORS = 32;

		Texture outputTex;

	private:
		UniformBuffer sensorBuf;
		Buffer readBackBuf;
		size_t readBackSize;
	};
}