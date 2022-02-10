#pragma once

#include "console/CFunc.hh"
#include "core/PreservingMap.hh"
#include "ecs/Ecs.hh"
#include "graphics/core/RenderTarget.hh"
#include "graphics/gui/MenuGuiManager.hh"
#include "graphics/opengl/GLBuffer.hh"
#include "graphics/opengl/GLModel.hh"
#include "graphics/opengl/GLTexture.hh"
#include "graphics/opengl/GPUTypes.hh"
#include "graphics/opengl/PerfTimer.hh"
#include "graphics/opengl/Shader.hh"
#include "graphics/opengl/gui/GuiRenderer.hh"

#include <atomic>
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

    struct VoxelContext {
        shared_ptr<GLRenderTarget> voxelCounters;
        shared_ptr<GLRenderTarget> fragmentListCurrent, fragmentListPrevious;
        shared_ptr<GLRenderTarget> voxelOverflow;
        shared_ptr<GLRenderTarget> radiance;
        shared_ptr<GLRenderTarget> radianceMips;

        int gridSize;
        float superSampleScale;
        float voxelSize;
        glm::vec3 voxelGridCenter;
        glm::vec3 gridMin, gridMax;
        ecs::VoxelArea areas[MAX_VOXEL_AREAS];

        void UpdateCache(ecs::Lock<ecs::Read<ecs::VoxelArea>> lock);
    };

    class VoxelRenderer {
    public:
        using DrawLock = typename ecs::Lock<ecs::Read<ecs::Renderable, ecs::Light, ecs::View, ecs::TransformSnapshot>,
            ecs::Write<ecs::Mirror>>;
        typedef std::function<void(DrawLock, Tecs::Entity &)> PreDrawFunc;

        VoxelRenderer(GlfwGraphicsContext &context, PerfTimer &timer);
        ~VoxelRenderer();

        // Functions inherited from Renderer
        void Prepare();
        // clang-format off
        void BeginFrame(ecs::Lock<ecs::Read<ecs::TransformSnapshot>,
                                  ecs::Write<ecs::Renderable,
                                             ecs::View,
                                             ecs::Light,
                                             ecs::LightSensor,
                                             ecs::Mirror,
                                             ecs::VoxelArea>> lock);
        // clang-format on
        void RenderPass(const ecs::View &view, DrawLock lock, RenderTarget *finalOutput = nullptr);
        void PrepareForView(const ecs::View &view);
        void EndFrame();

        // Functions specific to VoxelRenderer
        void PrepareGuis(DebugGuiManager *debugGui, MenuGuiManager *menuGui);
        void UpdateShaders(bool force = false);
        void RenderMainMenu(ecs::View &view, bool renderToGel = false);
        void RenderShadowMaps(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View, ecs::Light>,
            ecs::Write<ecs::Renderable, ecs::Mirror>> lock);
        void PrepareVoxelTextures();
        void RenderVoxelGrid(ecs::Lock<ecs::Read<ecs::Renderable, ecs::TransformSnapshot, ecs::View, ecs::Light>,
            ecs::Write<ecs::Mirror>> lock);
        void ReadBackLightSensors(ecs::Lock<ecs::Write<ecs::LightSensor>> lock);
        void UpdateLightSensors(
            ecs::Lock<ecs::Read<ecs::LightSensor, ecs::Light, ecs::View, ecs::TransformSnapshot>> lock);
        void ForwardPass(const ecs::View &view, SceneShader *shader, DrawLock lock, const PreDrawFunc &preDraw = {});
        void DrawEntity(const ecs::View &view,
            SceneShader *shader,
            DrawLock lock,
            Tecs::Entity &ent,
            const PreDrawFunc &preDraw = {});
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

        const LightingContext &Lighting() const {
            return lightContext;
        }

    private:
        PreservingMap<std::string, GLModel> activeModels;

        shared_ptr<GLRenderTarget> shadowMap;
        shared_ptr<GLRenderTarget> mirrorShadowMap;
        shared_ptr<GLRenderTarget> menuGuiTarget;
        GLBuffer indirectBufferCurrent, indirectBufferPrevious;
        VoxelContext voxelContext;
        GLBuffer mirrorVisData;
        GLBuffer mirrorSceneData;

        std::shared_ptr<GuiRenderer> debugGuiRenderer;
        std::shared_ptr<GuiRenderer> menuGuiRenderer;
        MenuGuiManager *menuGui = nullptr;

        LightingContext lightContext;

        std::atomic_bool reloadShaders;
        CFuncCollection funcs;
    };

    extern CVar<bool> CVarRenderWireframe;
    extern CVar<bool> CVarUpdateVoxels;
    extern CVar<int> CVarMirrorRecursion;
    extern CVar<int> CVarMirrorMapResolution;

    extern CVar<int> CVarVoxelGridSize;
    extern CVar<int> CVarShowVoxels;
    extern CVar<float> CVarVoxelSuperSample;
    extern CVar<bool> CVarEnableShadows;
    extern CVar<bool> CVarEnablePCF;
    extern CVar<bool> CVarEnableBumpMap;
} // namespace sp
