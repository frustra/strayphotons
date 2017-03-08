#include "graphics/Util.hh"
#include "graphics/VertexBuffer.hh"

namespace sp
{
	void DrawScreenCover(bool flipped)
	{
		if (flipped)
		{
			static TextureVertex screenCoverElements[] =
			{
				{{ -2, -1, 0}, { -0.5, 1}},
				{{2, -1, 0}, {1.5, 1}},
				{{0, 3, 0}, {0.5, -1}},
			};

			static VertexBuffer vbo;

			if (!vbo.Initialized())
			{
				vbo.SetElementsVAO(3, screenCoverElements);
			}

			vbo.BindVAO();
			glDrawArrays(GL_TRIANGLES, 0, vbo.Elements());
		}
		else
		{
			static TextureVertex screenCoverElements[] =
			{
				{{ -2, -1, 0}, { -0.5, 0}},
				{{2, -1, 0}, {1.5, 0}},
				{{0, 3, 0}, {0.5, 2}},
			};

			static VertexBuffer vbo;

			if (!vbo.Initialized())
			{
				vbo.SetElementsVAO(3, screenCoverElements);
			}

			vbo.BindVAO();
			glDrawArrays(GL_TRIANGLES, 0, vbo.Elements());
		}
	}
}
