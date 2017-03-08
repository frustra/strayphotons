#include "MenuGui.hh"
#include "graphics/Renderer.hh"

namespace sp
{
	void RenderMenuGui::Process(const PostProcessingContext *context)
	{
		auto r = context->renderer;
		auto view = context->view;
		auto target = GetInput(0)->GetOutput()->TargetRef;

		r->SetRenderTarget(&target->GetTexture(), nullptr);
		r->RenderMainMenu(view);

		SetOutputTarget(0, target);
	}
}
