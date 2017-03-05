#include "GuiRenderer.hh"
#include "Renderer.hh"
#include "GenericShaders.hh"
#include "ShaderManager.hh"
#include "GPUTimer.hh"
#include "game/InputManager.hh"
#include "game/GuiManager.hh"
#include "assets/AssetManager.hh"
#include "assets/Asset.hh"

#include <imgui/imgui.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw/glfw3native.h>
#endif

namespace sp
{
	GuiRenderer::GuiRenderer(Renderer &renderer, GuiManager &manager, string font)
		: parent(renderer), manager(manager)
	{
		imCtx = ImGui::CreateContext(nullptr, nullptr);
		ImGui::SetCurrentContext(imCtx);
		ImGuiIO &io = ImGui::GetIO();

		io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
		io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
		io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
		io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
		io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
		io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
		io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
		io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
		io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
		io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
		io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
		io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
		io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
		io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
		io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
		io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

#ifdef _WIN32
		io.ImeWindowHandle = glfwGetWin32Window(renderer.GetWindow());
#endif

		shared_ptr<Asset> fontAsset;

		if (font != "")
		{
			fontAsset = GAssets.Load(font);
			io.Fonts->AddFontFromMemoryTTF((void *)fontAsset->Buffer(), fontAsset->Size(), 16.0f);
		}

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
		.Storage(PF_RGBA8)
		.Image2D(fontData, 0, fontWidth, fontHeight);

		io.Fonts->TexID = (void *)(intptr_t) fontTex.handle;
	}

	GuiRenderer::~GuiRenderer()
	{
		ImGui::DestroyContext(imCtx);
		ImGui::Shutdown();
	}

	void GuiRenderer::Render(ecs::View view)
	{
		RenderPhase phase("GuiRender", parent.Timer);

		ImGui::SetCurrentContext(imCtx);
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
