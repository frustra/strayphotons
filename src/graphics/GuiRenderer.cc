#include "GuiRenderer.hh"
#include "Renderer.hh"
#include "GenericShaders.hh"
#include "ShaderManager.hh"
#include "GPUTimer.hh"
#include "game/InputManager.hh"
#include "game/GuiManager.hh"

#include <imgui/imgui.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw/glfw3native.h>
#endif

namespace sp
{
	GuiRenderer::GuiRenderer(Renderer &renderer, GuiManager &manager)
		: parent(renderer), manager(manager)
	{
		ImGuiIO &io = ImGui::GetIO();

#ifdef _WIN32
		io.ImeWindowHandle = glfwGetWin32Window(renderer.GetWindow());
#endif

#define OFFSET(typ, field) ((size_t) &(((typ *) nullptr)->field))

		vertices.Create().CreateVAO()
		.EnableAttrib(0, 2, GL_FLOAT, false, OFFSET(ImDrawVert, pos), sizeof(ImDrawVert))
		.EnableAttrib(1, 2, GL_FLOAT, false, OFFSET(ImDrawVert, uv), sizeof(ImDrawVert))
		.EnableAttrib(2, 4, GL_UNSIGNED_BYTE, true, OFFSET(ImDrawVert, col), sizeof(ImDrawVert));

		indices.Create();

#undef OFFSET

		unsigned char *fontData;
		int fontWidth, fontHeight;
		io.Fonts->GetTexDataAsRGBA32(&fontData, &fontWidth, &fontHeight);

		fontTex.Create().Filter(GL_LINEAR, GL_LINEAR)
		.Size(fontWidth, fontHeight)
		.Storage2D(PF_RGBA8)
		.Image2D(fontData, 0, fontWidth, fontHeight);

		io.Fonts->TexID = (void *)(intptr_t) fontTex.handle;
	}

	void GuiRenderer::Render(ECS::View view)
	{
		RenderPhase phase("GuiRender", parent.timer);

		ImGuiIO &io = ImGui::GetIO();

		io.DisplaySize = ImVec2((float)view.extents.x, (float)view.extents.y);

		double currTime = glfwGetTime();
		io.DeltaTime = lastTime > 0.0 ? (float)(currTime - lastTime) : 1.0f / 60.0f;
		lastTime = currTime;

		manager.BeforeFrame();
		ImGui::NewFrame();
		manager.DefineWindows();
		ImGui::Render();

		auto drawData = ImGui::GetDrawData();

		drawData->ScaleClipRects(io.DisplayFramebufferScale);

		glViewport(view.offset.x, view.offset.y, view.extents.x, view.extents.y);

		parent.GlobalShaders->Get<BasicOrthoVS>()->SetViewport(view.extents.x, view.extents.y);
		parent.ShaderControl->BindPipeline<BasicOrthoVS, BasicOrthoFS>(parent.GlobalShaders);

		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_SCISSOR_TEST);

		vertices.BindVAO();
		indices.BindElementArray();

		for (int i = 0; i < drawData->CmdListsCount; i++)
		{
			const auto cmdList = drawData->CmdLists[i];
			const ImDrawIdx *offset = 0;

			vertices.SetElements(cmdList->VtxBuffer.size(), &cmdList->VtxBuffer.front(), GL_STREAM_DRAW);
			indices.SetElements(cmdList->IdxBuffer.size(), &cmdList->IdxBuffer.front(), GL_STREAM_DRAW);

			for (const auto &pcmd : cmdList->CmdBuffer)
			{
				if (pcmd.UserCallback)
				{
					pcmd.UserCallback(cmdList, &pcmd);
				}
				else
				{
					auto texID = (GLuint)(intptr_t)pcmd.TextureId;
					glBindTextures(0, 1, &texID);

					glScissor(
						(int)pcmd.ClipRect.x,
						(int)(view.extents.y - pcmd.ClipRect.w),
						(int)(pcmd.ClipRect.z - pcmd.ClipRect.x),
						(int)(pcmd.ClipRect.w - pcmd.ClipRect.y)
					);

					GLenum elemType = sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
					glDrawElements(GL_TRIANGLES, pcmd.ElemCount, elemType, offset);
				}
				offset += pcmd.ElemCount;
			}
		}
	}
}
