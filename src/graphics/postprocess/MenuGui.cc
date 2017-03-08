#include "MenuGui.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"

namespace sp
{
	void RenderMenuGui::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto view = context->view;
		auto target = GetInput(0)->GetOutput()->TargetRef;
		auto blurred = GetInput(1)->GetOutput()->TargetRef;

		glViewport(0, 0, target->GetDesc().extent.x, target->GetDesc().extent.y);

		blurred->GetTexture().Bind(0);
		r->ShaderControl->BindPipeline<BasicPostVS, ScreenCoverFS>(r->GlobalShaders);
		r->SetRenderTarget(&target->GetTexture(), nullptr);
		DrawScreenCover();

		r->RenderMainMenu(view);

		SetOutputTarget(0, target);
	}
}
