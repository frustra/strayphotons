#pragma once

#include "core/CFunc.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/VoxelInfo.hh"
#include "graphics/core/Renderer.hh"
#include "graphics/opengl/GLBuffer.hh"
#include "graphics/opengl/GLTexture.hh"
#include "graphics/opengl/PerfTimer.hh"
#include "graphics/opengl/Shader.hh"
#include "graphics/opengl/gui/GuiRenderer.hh"
#include "graphics/opengl/gui/MenuGuiManager.hh"

#include <functional>
#include <glm/glm.hpp>

namespace physx {
    struct PxDebugLine;
}

namespace sp {
    class Game;
    class GLRenderTarget;
    class RenderTargetPool;
    class ShaderManager;
    class SceneShader;
    class Model;
    class GraphicsContext;
    class GlfwGraphicsContext;
    class DebugGuiManager;

    struct VoxelData {
        shared_ptr<GLRenderTarget> voxelCounters;
        shared_ptr<GLRenderTarget> fragmentListCurrent, fragmentListPrevious;
        shared_ptr<GLRenderTarget> voxelOverflow;
        shared_ptr<GLRenderTarget> radiance;
        shared_ptr<GLRenderTarget> radianceMips;
        ecs::VoxelInfo info;
    };

    class VoxelRenderer : public Renderer {
    public:
        typedef std::function<void(ecs::Lock<ecs::ReadAll>, Tecs::Entity &)> PreDrawFunc;

        VoxelRenderer(ecs::EntityManager &ecs, GlfwGraphicsContext &context, PerfTimer &timer);
        ~VoxelRenderer();

        // Functions inherited from Renderer
        void Prepare() override;
        void BeginFrame() override;
        void RenderPass(ecs::View view, RenderTarget *finalOutput = nullptr) override;
        void PrepareForView(const ecs::View &view) override;
        void RenderLoading(ecs::View view) override;
        void EndFrame() override;

        // Functions specific to VoxelRenderer
        void PrepareGuis(DebugGuiManager *debugGui, MenuGuiManager *menuGui);
        void UpdateShaders(bool force = false);
        void RenderMainMenu(ecs::View &view, bool renderToGel = false);
        void RenderShadowMaps();
        void PrepareVoxelTextures();
        void RenderVoxelGrid();
        void ReadBackLightSensors();
        void UpdateLightSensors();
        void ForwardPass(const ecs::View &view,
                         SceneShader *shader,
                         ecs::Lock<ecs::ReadAll> lock,
                         const PreDrawFunc &preDraw = {});
        void DrawEntity(const ecs::View &view,
                        SceneShader *shader,
                        ecs::Lock<ecs::ReadAll> lock,
                        Tecs::Entity &ent,
                        const PreDrawFunc &preDraw = {});
        void ExpireRenderables();
        void DrawPhysxLines(const ecs::View &view,
                            SceneShader *shader,
                            const vector<physx::PxDebugLine> &lines,
                            ecs::Lock<ecs::ReadAll> lock,
                            const PreDrawFunc &preDraw);
        void DrawGridDebug(const ecs::View &view, SceneShader *shader);
        void SetRenderTarget(GLRenderTarget *attachment0, GLRenderTarget *depth);
        void SetRenderTargets(size_t attachmentCount, GLRenderTarget **attachments, GLRenderTarget *depth);
        void SetDefaultRenderTarget();

        MenuRenderMode GetMenuRenderMode() const {
            return menuGui ? menuGui->RenderMode() : MenuRenderMode::None;
        }

        static void DrawScreenCover(bool flipped = false);

        ShaderManager *ShaderControl = nullptr;

        float Exposure = 1.0f;

        GlfwGraphicsContext &context;
        ShaderSet shaders;
        PerfTimer &timer;

    private:
        shared_ptr<GLRenderTarget> shadowMap;
        shared_ptr<GLRenderTarget> mirrorShadowMap;
        shared_ptr<GLRenderTarget> menuGuiTarget;
        GLBuffer indirectBufferCurrent, indirectBufferPrevious;
        VoxelData voxelData;
        GLBuffer mirrorVisData;
        GLBuffer mirrorSceneData;

        std::shared_ptr<GuiRenderer> debugGuiRenderer;
        std::shared_ptr<GuiRenderer> menuGuiRenderer;
        MenuGuiManager *menuGui = nullptr;

        ecs::Observer<ecs::Removed<ecs::Renderable>> renderableRemoval;
        std::deque<std::pair<shared_ptr<Model>, int>> renderableGCQueue;

        CFuncCollection funcs;
    };
} // namespace sp
