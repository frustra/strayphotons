#include "GPUTypes.hh"

#include <core/Logging.hh>
#include <ecs/EcsImpl.hh>
#include <graphics/opengl/voxel_renderer/VoxelRenderer.hh>

namespace sp {
    void FillLightData(LightingContext &lightData, ecs::Lock<ecs::Read<ecs::Light, ecs::Transform>> lock) {
        int lightCount = 0;
        glm::ivec2 renderTargetSize(0, 0);
        for (auto entity : lock.EntitiesWith<ecs::Light>()) {
            if (!entity.Has<ecs::Light, ecs::Transform>(lock)) continue;

            auto &light = entity.Get<ecs::Light>(lock);
            if (!light.on) continue;

            int extent = (int)std::pow(2, light.shadowMapSize);

            auto transform = entity.Get<ecs::Transform>(lock).GetGlobalTransform(lock);
            auto &view = lightData.views[lightCount];

            view.visibilityMask.set(ecs::Renderable::VISIBLE_LIGHTING_SHADOW);
            view.extents = {extent, extent};
            view.fov = light.spotAngle * 2.0f;
            view.offset = {renderTargetSize.x, 0};
            view.clearMode.reset();
            view.clip = light.shadowMapClip;
            view.UpdateProjectionMatrix();
            view.UpdateViewMatrix(lock, entity);

            auto &data = lightData.glData[lightCount];
            data.position = transform.GetPosition();
            data.tint = light.tint;
            data.direction = transform.GetForward();
            data.spotAngleCos = cos(light.spotAngle);
            data.proj = view.projMat;
            data.invProj = view.invProjMat;
            data.view = view.viewMat;
            data.clip = view.clip;
            data.mapOffset = {renderTargetSize.x, 0, extent, extent};
            data.intensity = light.intensity;
            data.illuminance = light.illuminance;
            data.gelId = light.gelName.empty() ? 0 : 1;

            renderTargetSize.x += extent;
            if (extent > renderTargetSize.y) renderTargetSize.y = extent;

            lightCount++;
            if (lightCount >= MAX_LIGHTS) break;
        }
        glm::vec4 mapOffsetScale(renderTargetSize, renderTargetSize);
        for (int i = 0; i < lightCount; i++) {
            lightData.glData[i].mapOffset /= mapOffsetScale;
        }
        lightData.renderTargetSize = renderTargetSize;
        lightData.lightCount = lightCount;
    }

    int FillMirrorData(GLMirrorData *data, ecs::Lock<ecs::Read<ecs::Mirror, ecs::Transform>> lock) {
        int mirrorNum = 0;
        for (auto entity : lock.EntitiesWith<ecs::Mirror>()) {
            if (!entity.Has<ecs::Mirror, ecs::Transform>(lock)) continue;

            auto &mirror = entity.Get<ecs::Mirror>(lock);
            auto transform = entity.Get<ecs::Transform>(lock).GetGlobalTransform(lock);
            data->modelMat = transform.GetTransform();
            data->size = mirror.size;

            glm::vec3 mirrorNormal = transform.GetForward();
            glm::vec3 mirrorPos = transform.GetPosition();

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

    void FillVoxelInfo(GLVoxelInfo *data, VoxelContext &source) {
        data->voxelSize = source.voxelSize;
        data->voxelGridCenter = source.voxelGridCenter;
        for (int i = 0; i < MAX_VOXEL_AREAS; i++) {
            data->areas[i].min = source.areas[i].min - glm::vec3(0.05);
            data->areas[i].max = source.areas[i].max + glm::vec3(0.05);
        }
    }
} // namespace sp
