#pragma once

#include "core/CFunc.hh"
#include "ecs/components/VoxelInfo.hh"
#include "graphics/Buffer.hh"
#include "graphics/Renderer.hh"
#include "graphics/Texture.hh"

#include <PxPhysicsAPI.h>
#include <ecs/Ecs.hh>
#include <functional>
#include <glm/glm.hpp>

namespace sp {
    class Game;
    class RenderTarget;
    class RenderTargetPool;
    class ShaderManager;
    class SceneShader;
    class Model;
    class GuiRenderer;

    struct VoxelData {
        shared_ptr<RenderTarget> voxelCounters;
        shared_ptr<RenderTarget> fragmentListCurrent, fragmentListPrevious;
        shared_ptr<RenderTarget> voxelOverflow;
        shared_ptr<RenderTarget> radiance;
        shared_ptr<RenderTarget> radianceMips;
        ecs::VoxelInfo info;
    };

    class VoxelRenderer : public Renderer {
    public:
        typedef std::function<void(ecs::Entity &)> PreDrawFunc;

        VoxelRenderer(Game *game, GlfwGraphicsContext& context);
        ~VoxelRenderer();

        // Functions inherited from Renderer            
        void Prepare() override;
        void BeginFrame() override;
        void RenderPass(ecs::View view, RenderTarget::Ref finalOutput = nullptr) override;
        void PrepareForView(const ecs::View &view) override;
        void RenderLoading(ecs::View view) override;
        void EndFrame() override;

        // Functions specific to VoxelRenderer
        // TODO: make all this private and ensure we can still compile
        void UpdateShaders(bool force = false);
        void RenderMainMenu(ecs::View &view, bool renderToGel = false);
        void RenderShadowMaps();
        void PrepareVoxelTextures();
        void RenderVoxelGrid();
        void ReadBackLightSensors();
        void UpdateLightSensors();
        void ForwardPass(const ecs::View &view, SceneShader *shader, const PreDrawFunc &preDraw = {});
        void DrawEntity(const ecs::View &view, SceneShader *shader, ecs::Entity &ent, const PreDrawFunc &preDraw = {});
        void ExpireRenderables();
        void DrawPhysxLines(const ecs::View &view,
                            SceneShader *shader,
                            const vector<physx::PxDebugLine> &lines,
                            const PreDrawFunc &preDraw);
        void DrawGridDebug(const ecs::View &view, SceneShader *shader);
        void SetRenderTarget(shared_ptr<RenderTarget> attachment0, shared_ptr<RenderTarget> depth);
        void SetRenderTargets(size_t attachmentCount,
                              shared_ptr<RenderTarget> *attachments,
                              shared_ptr<RenderTarget> depth);
        void SetDefaultRenderTarget();

        ShaderManager *ShaderControl = nullptr;
        RenderTargetPool *RTPool = nullptr;

        float Exposure = 1.0f;

    private:
        shared_ptr<RenderTarget> shadowMap;
        shared_ptr<RenderTarget> mirrorShadowMap;
        shared_ptr<RenderTarget> menuGuiTarget;
        Buffer indirectBufferCurrent, indirectBufferPrevious;
        VoxelData voxelData;
        Buffer mirrorVisData;
        Buffer mirrorSceneData;

        shared_ptr<GuiRenderer> debugGuiRenderer;
        shared_ptr<GuiRenderer> menuGuiRenderer;

        ecs::Observer<ecs::Removed<ecs::Renderable>> renderableRemoval;
        std::deque<std::pair<shared_ptr<Model>, int>> renderableGCQueue;

        CFuncCollection funcs;

        GlfwGraphicsContext& context;
    };
} // namespace sp
