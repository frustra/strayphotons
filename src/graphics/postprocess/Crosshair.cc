#include "Crosshair.hh"
#include "core/CVar.hh"
#include "graphics/Renderer.hh"
#include "graphics/ShaderManager.hh"
#include "graphics/GenericShaders.hh"
#include "graphics/Util.hh"

#include <random>

namespace sp
{
	static CVar<float> CVarCrosshairSpread("r.CrosshairSpread", 10.0f, "Distance between crosshair dots");
	static CVar<float> CVarCrosshairDotSize("r.CrosshairDotSize", 2.0f, "Size of crosshair dots");

	static void drawDots(glm::ivec2 offset, float spread, float size)
	{
		glViewport(offset.x, offset.y, size, size);
		DrawScreenCover();
		glViewport(offset.x + spread, offset.y, size, size);
		DrawScreenCover();
		glViewport(offset.x - spread, offset.y, size, size);
		DrawScreenCover();
		glViewport(offset.x, offset.y + spread, size, size);
		DrawScreenCover();
		glViewport(offset.x, offset.y - spread, size, size);
		DrawScreenCover();
	}

	void Crosshair::Process(const PostProcessingContext *context)
	{
		auto view = context->view;
		auto spread = CVarCrosshairSpread.Get();
		auto size = CVarCrosshairDotSize.Get();
		auto offset = view.extents / 2 - glm::ivec2(size / 2);

		static Texture tex1, tex2;

		if (!tex1.handle)
		{
			static unsigned char color[4] = { 255, 255, 255, 50 };
			tex1.Create().Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage(PF_RGBA8).Image2D(color);
		}

		if (!tex2.handle)
		{
			static unsigned char color[4] = { 150, 150, 150, 255 };
			tex2.Create().Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage(PF_RGBA8).Image2D(color);
		}

		glEnable(GL_BLEND);
		//glBlendFuncSeparate(GL_ONE_MINUS_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
		glBlendEquation(GL_FUNC_ADD);
		tex1.Bind(0);
		drawDots(offset, spread, size);

		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ZERO, GL_ZERO, GL_ONE);
		glBlendEquation(GL_MIN);
		tex2.Bind(0);
		drawDots(offset, spread, size);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquation(GL_FUNC_ADD);
		glDisable(GL_BLEND);
		glViewport(0, 0, view.extents.x, view.extents.y);

		SetOutputTarget(0, GetInput(0)->GetOutput()->TargetRef);
	}
}
