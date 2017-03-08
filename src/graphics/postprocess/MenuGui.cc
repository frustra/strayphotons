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

		r->SetRenderTarget(&GetInput(0)->GetOutput()->TargetRef->GetTexture(), nullptr);
		r->RenderMainMenu(view);

		SetOutputTarget(0, GetInput(0)->GetOutput()->TargetRef);
	}
}
