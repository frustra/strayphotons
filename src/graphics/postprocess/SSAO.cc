#include "SSAO.hh"
#include "core/Logging.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"

namespace sp
{
	class SSAOPass0VS : public Shader
	{
		SHADER_TYPE(SSAOPass0VS)
		using Shader::Shader;
	};

	class SSAOPass0FS : public Shader
	{
		SHADER_TYPE(SSAOPass0FS)
		using Shader::Shader;
	};

	IMPLEMENT_SHADER_TYPE(SSAOPass0VS, "ssao_pass0.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(SSAOPass0FS, "ssao_pass0.frag", Fragment);

	void SSAO::Process(const PostProcessingContext *context)
	{
		auto r = context->Renderer;
		auto dest = outputs[0].AllocateTarget(context)->GetTexture();

		r->SetRenderTarget(&dest, nullptr);
		r->ShaderManager->BindPipeline<SSAOPass0VS, SSAOPass0FS>(*r->ShaderSet);

		DrawScreenCover();
	}
}