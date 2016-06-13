#include "Lighting.hh"
#include "core/Logging.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"
#include "ecs/components/View.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/Transform.hh"

namespace sp
{
	class DeferredLightingFS : public Shader
	{
		SHADER_TYPE(DeferredLightingFS)

		static const int maxLights = 16;

		DeferredLightingFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
			Bind(lightCount, "lightCount");
			Bind(lightPosition, "lightPosition");
			Bind(lightTint, "lightTint");
			Bind(lightDirection, "lightDirection");

			Bind(viewMat, "viewMat");
			Bind(invViewMat, "invViewMat");
			Bind(invProjMat, "invProjMat");
		}

		void SetViewParams(const ECS::View &view)
		{
			Set(viewMat, view.viewMat);
			Set(invViewMat, view.invViewMat);
			Set(invProjMat, view.invProjMat);
		}

		void SetLights(EntityManager &manager, EntityManager::EntityCollection &lightCollection)
		{
			glm::vec3 lightPositions[maxLights];
			glm::vec3 lightTints[maxLights];
			glm::vec3 lightDirections[maxLights];
			int lightNum = 0;
			for (auto entity : lightCollection)
			{
				auto light = entity.Get<ECS::Light>();
				//auto view = entity.Get<ECS::View>();
				auto transform = entity.Get<ECS::Transform>();
				lightPositions[lightNum] = transform->GetModelTransform(manager) * glm::vec4(0, 0, 0, 1);
				lightTints[lightNum] = light->tint;
				lightDirections[lightNum] = glm::mat3(transform->GetModelTransform(manager)) * glm::vec3(0, 0, -1);
				lightNum++;
			}

			Set(lightCount, lightNum);
			Set(lightPosition, &lightPositions[0], lightNum);
			Set(lightTint, &lightTints[0], lightNum);
			Set(lightDirection, &lightDirections[0], lightNum);
		}

	private:
		Uniform lightCount, lightPosition, lightTint, lightDirection;
		Uniform viewMat, invViewMat, invProjMat;
	};

	IMPLEMENT_SHADER_TYPE(DeferredLightingFS, "deferred_lighting.frag", Fragment);

	void DeferredLighting::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context)->GetTexture();

		auto lights = context->game->entityManager.EntitiesWith<ECS::Light>();
		r->GlobalShaders->Get<DeferredLightingFS>()->SetLights(context->game->entityManager, lights);
		r->GlobalShaders->Get<DeferredLightingFS>()->SetViewParams(context->view);

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, DeferredLightingFS>(r->GlobalShaders);

		DrawScreenCover();
	}

	class TonemapFS : public Shader
	{
		SHADER_TYPE(TonemapFS)

		TonemapFS(shared_ptr<ShaderCompileOutput> compileOutput) : Shader(compileOutput)
		{
		}
	};

	IMPLEMENT_SHADER_TYPE(TonemapFS, "tonemap.frag", Fragment);

	void Tonemap::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto dest = outputs[0].AllocateTarget(context)->GetTexture();

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderControl->BindPipeline<BasicPostVS, TonemapFS>(r->GlobalShaders);

		DrawScreenCover();
	}
}
