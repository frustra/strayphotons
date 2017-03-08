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
		auto target = GetInput(0)->GetOutput()->TargetRef;
		auto blurred = GetInput(1)->GetOutput()->TargetRef;
		auto &dest = outputs[0].AllocateTarget(context)->GetTexture();

		glViewport(0, 0, target->GetDesc().extent.x, target->GetDesc().extent.y);

		blurred->GetTexture().Bind(0);
		r->ShaderControl->BindPipeline<BasicPostVS, ScreenCoverFS>(r->GlobalShaders);
		r->SetRenderTarget(&dest, nullptr);
		DrawScreenCover();

		auto view = context->view;
		r->RenderMainMenu(view);
	}
}
