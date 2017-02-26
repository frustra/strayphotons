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
			if (lightNum >= MAX_LIGHTS) break;
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

			glm::vec3 mirrorNormal = glm::mat3(data->modelMat) * glm::vec3(0, 0, -1);
			glm::vec3 mirrorPos = glm::vec3(data->modelMat * glm::vec4(0, 0, 0, 1));

			float d = -glm::dot(mirrorNormal, mirrorPos);
			data->reflectMat = glm::mat4(glm::mat3(1) - 2.0f * glm::outerProduct(mirrorNormal, mirrorNormal));
			data->reflectMat[3] = glm::vec4(-2.0f * d * mirrorNormal, 1);
			data->plane = glm::vec4(mirrorNormal, d);

			mirrorNum++;
			if (mirrorNum >= MAX_MIRRORS) break;
			data++;
		}
		return mirrorNum;
	}
}
