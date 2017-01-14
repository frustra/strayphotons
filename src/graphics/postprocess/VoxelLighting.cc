#include "VoxelLighting.hh"
#include "core/Logging.hh"
#include "core/CVar.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/Transform.hh"

namespace sp
{
	static CVar<int> CVarVoxelLightingDebug("r.VoxelLightingDebug", 0, "Show unprocessed Voxel lighting (1: combined, 2: diffuse, 3: specular, 4: AO)");

	class VoxelLightingFS : public Shader
	{
		SHADER_TYPE(VoxelLightingFS)

		static const int maxLights = 16;

		VoxelLightingFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(lightCount, "lightCount");
			Bind(lightPosition, "lightPosition");
			Bind(lightTint, "lightTint");
			Bind(lightDirection, "lightDirection");
			Bind(lightSpotAngleCos, "lightSpotAngleCos");
			Bind(lightProj, "lightProj");
			Bind(lightView, "lightView");
			Bind(lightClip, "lightClip");
			Bind(lightMapOffset, "lightMapOffset");
			Bind(lightIntensity, "lightIntensity");
			Bind(lightIlluminance, "lightIlluminance");

			Bind(exposure, "exposure");
			Bind(targetSize, "targetSize");

			Bind(invProjMat, "invProjMat");
			Bind(invViewMat, "invViewMat");
			Bind(debug, "debug");

			Bind(voxelSize, "voxelSize");
			Bind(voxelGridCenter, "voxelGridCenter");
		}

		void SetLights(ecs::EntityManager &manager, ecs::EntityManager::EntityCollection &lightCollection)
		{
			glm::vec3 lightPositions[maxLights];
			glm::vec3 lightTints[maxLights];
			glm::vec3 lightDirections[maxLights];
			float lightSpotAnglesCos[maxLights];
			glm::mat4 lightProjs[maxLights];
			glm::mat4 lightViews[maxLights];
			glm::vec2 lightClips[maxLights];
			glm::vec4 lightMapOffsets[maxLights];
			float lightIntensities[maxLights];
			float lightIlluminances[maxLights];

			int lightNum = 0;
			for (auto entity : lightCollection)
			{
				auto light = entity.Get<ecs::Light>();
				auto view = entity.Get<ecs::View>();
				auto transform = entity.Get<ecs::Transform>();
				lightPositions[lightNum] = transform->GetModelTransform(manager) * glm::vec4(0, 0, 0, 1);
				lightTints[lightNum] = light->tint;
				lightDirections[lightNum] = glm::mat3(transform->GetModelTransform(manager)) * glm::vec3(0, 0, -1);
				lightSpotAnglesCos[lightNum] = cos(light->spotAngle);
				lightProjs[lightNum] = view->projMat;
				lightViews[lightNum] = view->viewMat;
				lightClips[lightNum] = view->clip;
				lightMapOffsets[lightNum] = light->mapOffset;
				lightIntensities[lightNum] = light->intensity;
				lightIlluminances[lightNum] = light->illuminance;
				lightNum++;
			}

			Set(lightCount, lightNum);
			Set(lightPosition, lightPositions, lightNum);
			Set(lightTint, lightTints, lightNum);
			Set(lightDirection, lightDirections, lightNum);
			Set(lightSpotAngleCos, lightSpotAnglesCos, lightNum);
			Set(lightProj, lightProjs, lightNum);
			Set(lightView, lightViews, lightNum);
			Set(lightClip, lightClips, lightNum);
			Set(lightMapOffset, lightMapOffsets, lightNum);
			Set(lightIntensity, lightIntensities, lightNum);
			Set(lightIlluminance, lightIlluminances, lightNum);
		}

		void SetExposure(float newExposure)
		{
			Set(exposure, newExposure);
		}

		void SetViewParams(const ecs::View &view)
		{
			Set(invProjMat, view.invProjMat);
			Set(invViewMat, view.invViewMat);
			Set(targetSize, glm::vec2(view.extents));
		}

		void SetDebug(int newDebug)
		{
			Set(debug, newDebug);
		}

		void SetVoxelInfo(ecs::VoxelInfo &voxelInfo)
		{
			Set(voxelSize, voxelInfo.voxelSize);
			Set(voxelGridCenter, voxelInfo.voxelGridCenter);
		}

	private:
		Uniform lightCount, lightPosition, lightTint, lightDirection, lightSpotAngleCos;
		Uniform lightProj, lightView, lightClip, lightMapOffset, lightIntensity, lightIlluminance;
		Uniform exposure, targetSize, invViewMat, invProjMat, debug;
		Uniform voxelSize, voxelGridCenter;
	};

	IMPLEMENT_SHADER_TYPE(VoxelLightingFS, "voxel_lighting.frag", Fragment);

	void VoxelLighting::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context)->GetTexture();

		auto lights = context->game->entityManager.EntitiesWith<ecs::Light>();
		r->GlobalShaders->Get<VoxelLightingFS>()->SetLights(context->game->entityManager, lights);
		r->GlobalShaders->Get<VoxelLightingFS>()->SetExposure(0.1);
		r->GlobalShaders->Get<VoxelLightingFS>()->SetViewParams(context->view);
		r->GlobalShaders->Get<VoxelLightingFS>()->SetDebug(CVarVoxelLightingDebug.Get());
		r->GlobalShaders->Get<VoxelLightingFS>()->SetVoxelInfo(context->renderer->voxelInfo);

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, VoxelLightingFS>(r->GlobalShaders);

		DrawScreenCover();
	}
}
