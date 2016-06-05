#pragma once

#include "PostProcess.hh"
#include "core/Logging.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"

namespace sp
{
	class SSAO : public PostProcessPass<2, 1>
	{
	public:
		void Process(const PostProcessingContext *context)
		{
			auto dest = outputs[0].AllocateTarget(context)->GetTexture();

			context->Renderer->SetRenderTarget(&dest, nullptr);
			context->Renderer->ShaderManager->BindPipeline<BasicPostVS, ScreenCoverFS>(*context->Renderer->ShaderSet);

			DrawScreenCover();
		}

		RenderTargetDesc GetOutputDesc(uint32 id)
		{
			return GetInput(id)->GetOutput()->RenderTargetDesc;
		}
	};
}