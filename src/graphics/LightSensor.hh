#pragma once

#include "graphics/Buffer.hh"
#include "graphics/Texture.hh"
#include "graphics/Shader.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GPUTypes.hh"
#include "ecs/components/LightSensor.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/VoxelInfo.hh"
#include <Ecs.hh>

namespace sp
{
	class LightSensorUpdateCS : public Shader
	{
		SHADER_TYPE(LightSensorUpdateCS);
		LightSensorUpdateCS(shared_ptr<ShaderCompileOutput> compileOutput);

		void SetSensors(ecs::EntityManager::EntityCollection &sensors);
		void SetLightData(int count, GLLightData *data);
		void SetVoxelInfo(GLVoxelInfo *data);
		void StartReadback();
		void UpdateValues(ecs::EntityManager &manager);

		static const int MAX_SENSORS = 32;

		Texture outputTex;

	private:
		UniformBuffer sensorData, lightData, voxelInfo;
		Uniform lightCount;
		Buffer readBackBuf;
		size_t readBackSize;
	};
}
