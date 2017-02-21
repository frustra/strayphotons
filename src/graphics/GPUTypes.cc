#include "GPUTypes.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/Mirror.hh"
#include "ecs/components/Transform.hh"

namespace sp
{
	int FillLightData(GLLightData *data, ecs::EntityManager &manager)
	{
		int lightNum = 0;
		for (auto entity : manager.EntitiesWith<ecs::Light>())
		{
			auto light = entity.Get<ecs::Light>();
			auto view = entity.Get<ecs::View>();
			auto transform = entity.Get<ecs::Transform>();
			data->position = transform->GetModelTransform(manager) * glm::vec4(0, 0, 0, 1);
			data->tint = light->tint;
			data->direction = glm::mat3(transform->GetModelTransform(manager)) * glm::vec3(0, 0, -1);
			data->spotAngleCos = cos(light->spotAngle);
			data->proj = view->projMat;
			data->invProj = view->invProjMat;
			data->view = view->viewMat;
			data->clip = view->clip;
			data->mapOffset = light->mapOffset;
			data->intensity = light->intensity;
			data->illuminance = light->illuminance;
			lightNum++;
			data++;
		}
		return lightNum;
	}

	int FillMirrorData(GLMirrorData *data, ecs::EntityManager &manager)
	{
		int mirrorNum = 0;
		for (auto entity : manager.EntitiesWith<ecs::Mirror>())
		{
			auto mirror = entity.Get<ecs::Mirror>();
			auto transform = entity.Get<ecs::Transform>();
			data->modelMat = transform->GetModelTransform(manager);
			data->size = mirror->size;
			mirrorNum++;
			data++;
		}
		return mirrorNum;
	}
}
