#include "graphics/GenericShaders.hh"

namespace sp
{
	IMPLEMENT_SHADER_TYPE(BasicPostVS, "basic_post.vert", Vertex);
	IMPLEMENT_SHADER_TYPE(ScreenCoverFS, "screen_cover.frag", Fragment);
}