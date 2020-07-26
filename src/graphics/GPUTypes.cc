#include "GPUTypes.hh"

#include "ecs/components/Light.hh"
#include "ecs/components/Mirror.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/View.hh"
#include "ecs/components/VoxelInfo.hh"

namespace sp {
	int FillLightData(GLLightData *data, ecs::EntityManager &manager) {
		int lightNum = 0;
		for (auto entity : manager.EntitiesWith<ecs::Light>()) {
			auto light = entity.Get<ecs::Light>();
			if (!light->on) {
				continue;
			}

			if (entity.Has<ecs::View>()) {
				auto view = entity.Get<ecs::View>();
				auto transform = entity.Get<ecs::Transform>();
				data->position = transform->GetGlobalTransform(manager) * glm::vec4(0, 0, 0, 1);
				data->tint = light->tint;
				data->direction = glm::mat3(transform->GetGlobalTransform(manager)) * glm::vec3(0, 0, -1);
				data->spotAngleCos = cos(light->spotAngle);
				data->proj = view->projMat;
				data->invProj = view->invProjMat;
				data->view = view->viewMat;
				data->clip = view->clip;
				data->mapOffset = light->mapOffset;
				data->intensity = light->intensity;
				data->illuminance = light->illuminance;
				data->gelId = light->gelId;
				lightNum++;
				if (lightNum >= MAX_LIGHTS)
					break;
				data++;
			}
		}
		return lightNum;
	}

	int FillMirrorData(GLMirrorData *data, ecs::EntityManager &manager) {
		int mirrorNum = 0;
		for (auto entity : manager.EntitiesWith<ecs::Mirror>()) {
			auto mirror = entity.Get<ecs::Mirror>();
			auto transform = entity.Get<ecs::Transform>();
			data->modelMat = transform->GetGlobalTransform(manager);
			data->size = mirror->size;

			glm::vec3 mirrorNormal = glm::mat3(data->modelMat) * glm::vec3(0, 0, -1);
			glm::vec3 mirrorPos = glm::vec3(data->modelMat * glm::vec4(0, 0, 0, 1));

			float d = -glm::dot(mirrorNormal, mirrorPos);
			data->reflectMat = glm::mat4(glm::mat3(1) - 2.0f * glm::outerProduct(mirrorNormal, mirrorNormal));
			data->reflectMat[3] = glm::vec4(-2.0f * d * mirrorNormal, 1);
			data->plane = glm::vec4(mirrorNormal, d);

			mirrorNum++;
			if (mirrorNum >= MAX_MIRRORS)
				break;
			data++;
		}
		return mirrorNum;
	}

	void FillVoxelInfo(GLVoxelInfo *data, ecs::VoxelInfo &source) {
		data->voxelSize = source.voxelSize;
		data->voxelGridCenter = source.voxelGridCenter;
		for (int i = 0; i < MAX_VOXEL_AREAS; i++) {
			data->areas[i].min = source.areas[i].min - glm::vec3(0.05);
			data->areas[i].max = source.areas[i].max + glm::vec3(0.05);
		}
	}
} // namespace sp
