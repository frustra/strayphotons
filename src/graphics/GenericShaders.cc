#include "graphics/GenericShaders.hh"

namespace sp {
	IMPLEMENT_SHADER_TYPE(BasicPostVS, "basic_post.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(ScreenCoverFS, "screen_cover.frag", Fragment);
	IMPLEMENT_SHADER_TYPE(ScreenCoverNoAlphaFS, "screen_cover_no_alpha.frag", Fragment);

	IMPLEMENT_SHADER_TYPE(BasicOrthoVS, "basic_ortho.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(BasicOrthoFS, "basic_ortho.frag", Fragment);

	IMPLEMENT_SHADER_TYPE(CopyStencilFS, "copy_stencil.frag", Fragment);

	IMPLEMENT_SHADER_TYPE(TextureFactorCS, "texture_factor.comp", Compute);
} // namespace sp