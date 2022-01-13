#include "VoxelRenderer.hh"

#include "assets/AssetManager.hh"
#include "console/CVar.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/gui/DebugGuiManager.hh"
#include "graphics/gui/MenuGuiManager.hh"
#include "graphics/opengl/GLModel.hh"
#include "graphics/opengl/GLView.hh"
#include "graphics/opengl/GenericShaders.hh"
#include "graphics/opengl/GlfwGraphicsContext.hh"
#include "graphics/opengl/LightSensor.hh"
#include "graphics/opengl/PerfTimer.hh"
#include "graphics/opengl/RenderTargetPool.hh"
#include "graphics/opengl/SceneShaders.hh"
#include "graphics/opengl/Shader.hh"
#include "graphics/opengl/ShaderManager.hh"
#include "graphics/opengl/VertexBuffer.hh"
#include "graphics/opengl/postprocess/PostProcess.hh"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/transform.hpp>
#include <vector>

namespace sp {
    VoxelRenderer::VoxelRenderer(GlfwGraphicsContext &context, PerfTimer &timer) : context(context), timer(timer) {}

    VoxelRenderer::~VoxelRenderer() {
        if (ShaderControl) delete ShaderControl;
    }

    const int MAX_MIRROR_RECURSION = 10;

    CVar<bool> CVarRenderWireframe("r.Wireframe", false, "Render wireframes");
    CVar<bool> CVarUpdateVoxels("r.UpdateVoxels", true, "Render voxel grid each frame");
    CVar<int> CVarMirrorRecursion("r.MirrorRecursion", 2, "Mirror recursion depth");
    CVar<int> CVarMirrorMapResolution("r.MirrorMapResolution", 512, "Resolution of mirror shadow maps");

    CVar<int> CVarVoxelGridSize("r.VoxelGridSize", 256, "NxNxN voxel grid dimensions");
    CVar<int> CVarShowVoxels("r.ShowVoxels", 0, "Show a wireframe of voxels at level N");
    CVar<float> CVarVoxelSuperSample("r.VoxelSuperSample", 1.0, "Render voxel grid with Nx supersampling");
    CVar<bool> CVarEnableShadows("r.EnableShadows", true, "Enable shadow mapping");
    CVar<bool> CVarEnablePCF("r.EnablePCF", true, "Enable smooth shadow sampling");
    CVar<bool> CVarEnableBumpMap("r.EnableBumpMap", true, "Enable bump mapping");

    void VoxelContext::UpdateCache(ecs::Lock<ecs::Read<ecs::VoxelArea>> lock) {

        this->gridMin = glm::vec3(0);
        this->gridMax = glm::vec3(0);
        int areaIndex = 0;
        for (auto ent : lock.EntitiesWith<ecs::VoxelArea>()) {
            if (areaIndex >= sp::MAX_VOXEL_AREAS) break;

            auto &area = ent.Get<ecs::VoxelArea>(lock);
            if (!areaIndex) {
                this->gridMin = area.min;
                this->gridMax = area.max;
            } else {
                this->gridMin = glm::min(this->gridMin, area.min);
                this->gridMax = glm::max(this->gridMax, area.max);
            }
            this->areas[areaIndex++] = area;
        }
        for (; areaIndex < sp::MAX_VOXEL_AREAS; areaIndex++) {
            this->areas[areaIndex] = {glm::vec3(0), glm::vec3(-1)};
        }

        this->gridSize = CVarVoxelGridSize.Get();
        this->superSampleScale = CVarVoxelSuperSample.Get();
        this->voxelGridCenter = (this->gridMin + this->gridMax) * glm::vec3(0.5);
        this->voxelSize = glm::compMax(this->gridMax - this->gridMin + glm::vec3(0.1)) / this->gridSize;
    }

    void VoxelRenderer::UpdateShaders(bool force) {
        if (force || CVarVoxelGridSize.Changed() || CVarVoxelSuperSample.Changed() || CVarEnableShadows.Changed() ||
            CVarEnablePCF.Changed() || CVarEnableBumpMap.Changed()) {
            int voxelGridSize = CVarVoxelGridSize.Get(true);
            ShaderManager::SetDefine("VOXEL_GRID_SIZE", std::to_string(voxelGridSize));
            ShaderManager::SetDefine("VOXEL_MIP_LEVELS", std::to_string(ceil(log2(voxelGridSize))));
            ShaderManager::SetDefine("VOXEL_SUPER_SAMPLE_SCALE", std::to_string(CVarVoxelSuperSample.Get(true)));
            ShaderManager::SetDefine("SHADOWS_ENABLED", CVarEnableShadows.Get(true));
            ShaderManager::SetDefine("PCF_ENABLED", CVarEnablePCF.Get(true));
            ShaderManager::SetDefine("BUMP_MAP_ENABLED", CVarEnableBumpMap.Get(true));
            ShaderControl->CompileAll(shaders);
        }
    }

    void VoxelRenderer::Prepare() {
        Assert(GLEW_ARB_compute_shader, "ARB_compute_shader required");
        Assert(GLEW_ARB_direct_state_access, "ARB_direct_state_access required");
        Assert(GLEW_ARB_multi_bind, "ARB_multi_bind required");
        Assert(!ShaderControl, "Renderer already prepared");

        // glEnable(GL_FRAMEBUFFER_SRGB);

        ShaderControl = new ShaderManager(shaders);
        ShaderManager::SetDefine("MAX_LIGHTS", std::to_string(MAX_LIGHTS));
        ShaderManager::SetDefine("MAX_MIRRORS", std::to_string(MAX_MIRRORS));
        ShaderManager::SetDefine("MAX_MIRROR_RECURSION", std::to_string(MAX_MIRROR_RECURSION));
        ShaderManager::SetDefine("MAX_LIGHT_SENSORS", std::to_string(MAX_LIGHT_SENSORS));
        ShaderManager::SetDefine("MAX_VOXEL_AREAS", std::to_string(MAX_VOXEL_AREAS));
        UpdateShaders(true);

        funcs.Register("reloadshaders", "Recompile all shaders", [&]() {
            UpdateShaders(true);
        });

        AssertGLOK("Renderer::Prepare");
    }

    void VoxelRenderer::PrepareGuis(DebugGuiManager *debugGui, MenuGuiManager *menuGui) {
        if (debugGui && !debugGuiRenderer) { debugGuiRenderer = make_shared<GuiRenderer>(*this, context, debugGui); }
        if (menuGui && !menuGuiRenderer) {
            this->menuGui = menuGui;
            menuGuiRenderer = make_shared<GuiRenderer>(*this, context, menuGui);
        }
    }

    void VoxelRenderer::RenderMainMenu(ecs::View &view, bool renderToGel) {
        if (renderToGel) {
            RenderTargetDesc menuDesc(PF_RGBA8, view.extents);
            menuDesc.levels = GLTexture::FullyMipmap;
            menuDesc.anisotropy = 4.0;
            if (!menuGuiTarget || menuGuiTarget->GetDesc() != menuDesc) {
                menuGuiTarget = context.GetRenderTarget(menuDesc);
            }

            SetRenderTarget(menuGuiTarget.get(), nullptr);
            PrepareForView(view);
            menuGuiRenderer->Render(view);
            menuGuiTarget->GetGLTexture().GenMipmap();
        } else {
            menuGuiRenderer->Render(view);
        }
    }

    void VoxelRenderer::RenderShadowMaps(
        ecs::Lock<ecs::Read<ecs::Transform, ecs::View, ecs::Light>, ecs::Write<ecs::Renderable, ecs::Mirror>> lock) {
        RenderPhase phase("ShadowMaps", timer);

        for (auto &entity : lock.EntitiesWith<ecs::Light>()) {
            auto &light = entity.Get<ecs::Light>(lock);

            if (light.bulb && light.bulb.Has<ecs::Renderable>(lock)) {
                auto &bulb = light.bulb.Get<ecs::Renderable>(lock);
                bulb.emissive = light.on ? light.intensity * light.tint * 0.1f : glm::vec3(0.0f);
            }
        }

        int mirrorCount = 0;
        for (auto &entity : lock.EntitiesWith<ecs::Mirror>()) {
            auto &mirror = entity.Get<ecs::Mirror>(lock);
            mirror.mirrorId = mirrorCount++;
        }

        FillLightData(lightContext, lock);

        RenderTargetDesc shadowDesc(PF_R32F, glm::max(glm::ivec2(1), lightContext.renderTargetSize));
        if (!shadowMap || shadowMap->GetDesc() != shadowDesc) { shadowMap = context.GetRenderTarget(shadowDesc); }

        if (!mirrorVisData) {
            // int count[4];
            // uint mask[MAX_LIGHTS * MAX_MIRRORS][MAX_MIRRORS];
            // uint list[MAX_LIGHTS * MAX_MIRRORS];
            // int sourceLight[MAX_LIGHTS * MAX_MIRRORS];
            // mat4 viewMat[MAX_LIGHTS * MAX_MIRRORS];
            // mat4 invViewMat[MAX_LIGHTS * MAX_MIRRORS];
            // mat4 projMat[MAX_LIGHTS * MAX_MIRRORS];
            // mat4 invProjMat[MAX_LIGHTS * MAX_MIRRORS];
            // mat4 lightViewMat[MAX_LIGHTS * MAX_MIRRORS];
            // mat4 invLightViewMat[MAX_LIGHTS * MAX_MIRRORS];
            // vec2 clip[MAX_LIGHTS * MAX_MIRRORS];
            // vec4 nearInfo[MAX_LIGHTS * MAX_MIRRORS];

            mirrorVisData.Create().Data(
                sizeof(GLint) * 4 + (sizeof(GLuint) * MAX_MIRRORS + sizeof(GLuint) * 8 + sizeof(glm::mat4) * 6) *
                                        (MAX_LIGHTS * MAX_MIRRORS + 1 /* padding */),
                nullptr,
                GL_DYNAMIC_COPY);
        }

        if (lightContext.lightCount == 0) return;

        const auto &renderTargetSize = lightContext.renderTargetSize;

        if (CVarEnableShadows.Get()) {
            auto depthTarget = context.GetRenderTarget({PF_DEPTH16, renderTargetSize, true});
            SetRenderTarget(shadowMap.get(), depthTarget.get());

            glViewport(0, 0, renderTargetSize.x, renderTargetSize.y);
            glDisable(GL_SCISSOR_TEST);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glClear(GL_DEPTH_BUFFER_BIT);

            mirrorVisData.Clear(PF_R32UI, 0);
            mirrorVisData.Bind(GL_SHADER_STORAGE_BUFFER, 0);

            for (int lightId = 0; lightId < lightContext.lightCount; lightId++) {
                auto &view = lightContext.views[lightId];

                ShaderControl->BindPipeline<ShadowMapVS, ShadowMapFS>();

                auto shadowMapVS = shaders.Get<ShadowMapVS>();
                auto shadowMapFS = shaders.Get<ShadowMapFS>();
                shadowMapFS->SetClip(view.clip);
                shadowMapFS->SetLight(lightId);
                ForwardPass(view, shadowMapVS, lock, [&](auto lock, Tecs::Entity &ent) {
                    if (ent && ent.Has<ecs::Mirror>(lock)) {
                        auto &mirror = ent.Get<ecs::Mirror>(lock);
                        shadowMapFS->SetMirrorId(mirror.mirrorId);
                    } else {
                        shadowMapFS->SetMirrorId(-1);
                    }
                });
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            }
        }

        glDisable(GL_CULL_FACE);

        GLMirrorData mirrorData[MAX_MIRRORS];
        int mirrorDataCount = FillMirrorData(&mirrorData[0], lock);
        int recursion = mirrorCount == 0 ? 0 : CVarMirrorRecursion.Get();

        int mapCount = lightContext.lightCount * mirrorDataCount * recursion;
        auto mapResolution = CVarMirrorMapResolution.Get();
        auto mirrorMapResolution = glm::ivec3(mapResolution, mapResolution, std::max(1, mapCount));
        RenderTargetDesc mirrorMapDesc(PF_R32F, mirrorMapResolution);
        mirrorMapDesc.textureArray = true;
        if (!mirrorShadowMap || mirrorShadowMap->GetDesc() != mirrorMapDesc) {
            mirrorShadowMap = context.GetRenderTarget(mirrorMapDesc);
        }

        for (int bounce = 0; bounce < recursion; bounce++) {
            {
                RenderPhase subPhase("MatrixGen", timer);

                auto mirrorMapCS = shaders.Get<MirrorMapCS>();

                mirrorMapCS->SetLightData(lightContext.lightCount, lightContext.glData);
                mirrorMapCS->SetMirrorData(mirrorDataCount, &mirrorData[0]);

                ShaderControl->BindPipeline<MirrorMapCS>();
                glDispatchCompute(1, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            }

            {
                RenderPhase subPhase("MirrorMaps", timer);

                RenderTargetDesc depthDesc(PF_DEPTH16, mirrorMapResolution);
                depthDesc.textureArray = true;
                auto depthTarget = context.GetRenderTarget(depthDesc);
                SetRenderTarget(mirrorShadowMap.get(), depthTarget.get());

                ecs::View basicView;
                basicView.extents = glm::ivec2(mapResolution);
                basicView.visibilityMask.set(ecs::Renderable::VISIBLE_REFLECTED);
                basicView.visibilityMask.set(ecs::Renderable::VISIBLE_LIGHTING_SHADOW);

                if (bounce > 0) { basicView.clearMode.reset(); }

                ShaderControl->BindPipeline<MirrorMapVS, MirrorMapGS, MirrorMapFS>();

                auto mirrorMapFS = shaders.Get<MirrorMapFS>();
                auto mirrorMapVS = shaders.Get<MirrorMapVS>();

                mirrorMapFS->SetLightData(lightContext.lightCount, lightContext.glData);
                mirrorMapFS->SetMirrorId(-1);

                shadowMap->GetGLTexture().Bind(4);
                mirrorShadowMap->GetGLTexture().Bind(5);

                ForwardPass(basicView, mirrorMapVS, lock, [&](auto lock, Tecs::Entity &ent) {
                    if (bounce == recursion - 1) {
                        // Don't mark mirrors on last pass.
                    } else if (ent && ent.Has<ecs::Mirror>(lock)) {
                        auto &mirror = ent.Get<ecs::Mirror>(lock);
                        mirrorMapFS->SetMirrorId(mirror.mirrorId);
                    } else {
                        mirrorMapFS->SetMirrorId(-1);
                    }
                });
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            }
        }
    }

    void VoxelRenderer::ReadBackLightSensors(ecs::Lock<ecs::Write<ecs::LightSensor>> lock) {
        RenderPhase phase("ReadBackLightSensors", timer);
        shaders.Get<LightSensorUpdateCS>()->UpdateValues(lock);
    }

    void VoxelRenderer::UpdateLightSensors(
        ecs::Lock<ecs::Read<ecs::LightSensor, ecs::Light, ecs::View, ecs::Transform>> lock) {
        RenderPhase phase("UpdateLightSensors", timer);
        auto shader = shaders.Get<LightSensorUpdateCS>();

        GLVoxelInfo voxelInfo;
        FillVoxelInfo(&voxelInfo, voxelContext);

        shader->SetSensors(lock);
        shader->SetLightData(lightContext.lightCount, lightContext.glData);
        shader->SetVoxelInfo(&voxelInfo);

        shader->outputTex.Clear(0);
        shader->outputTex.BindImage(0, GL_WRITE_ONLY);

        mirrorVisData.Bind(GL_SHADER_STORAGE_BUFFER, 0);
        voxelContext.radiance->GetGLTexture().Bind(0);
        voxelContext.radianceMips->GetGLTexture().Bind(1);
        shadowMap->GetGLTexture().Bind(2);
        if (mirrorShadowMap) mirrorShadowMap->GetGLTexture().Bind(3);

        ShaderControl->BindPipeline<LightSensorUpdateCS>();
        glDispatchCompute(1, 1, 1);

        shader->StartReadback();
    }

    void VoxelRenderer::RenderPass(const ecs::View &view, DrawLock lock, RenderTarget *finalOutput) {
        RenderPhase phase("RenderPass", timer);

        if (!mirrorSceneData) {
            // int count[4];
            // uint mask[SCENE_MIRROR_LIST_SIZE][MAX_MIRRORS];
            // uint list[SCENE_MIRROR_LIST_SIZE];
            // int sourceIndex[SCENE_MIRROR_LIST_SIZE];
            // mat4 reflectMat[SCENE_MIRROR_LIST_SIZE];
            // mat4 invReflectMat[SCENE_MIRROR_LIST_SIZE];
            // vec4 clipPlane[SCENE_MIRROR_LIST_SIZE];

            mirrorSceneData.Create().Data(
                sizeof(GLint) * 4 + (sizeof(GLuint) * MAX_MIRRORS + sizeof(GLuint) * 6 + sizeof(glm::mat4) * 2) *
                                        (MAX_MIRRORS * MAX_MIRROR_RECURSION + 1 /* padding */),
                nullptr,
                GL_DYNAMIC_COPY);
        }

        mirrorSceneData.Clear(PF_R32UI, 0);

        EngineRenderTargets targets;
        targets.gBuffer0 = context.GetRenderTarget({PF_RGBA8, view.extents});
        targets.gBuffer1 = context.GetRenderTarget({PF_RGBA16F, view.extents});
        targets.gBuffer2 = context.GetRenderTarget({PF_RGBA16F, view.extents});
        targets.gBuffer3 = context.GetRenderTarget({PF_RGBA8, view.extents});
        targets.shadowMap = shadowMap;
        targets.mirrorShadowMap = mirrorShadowMap;
        targets.voxelContext = voxelContext;
        targets.mirrorVisData = mirrorVisData;
        targets.mirrorSceneData = mirrorSceneData;
        targets.lightingGel = menuGuiTarget;

        if (finalOutput) { targets.finalOutput = finalOutput; }

        {
            RenderPhase subPhase("PlayerView", timer);

            auto mirrorIndexStencil0 = context.GetRenderTarget({PF_R32UI, view.extents});
            auto mirrorIndexStencil1 = context.GetRenderTarget({PF_R32UI, view.extents});

            const int attachmentCount = 5;

            GLRenderTarget *attachments[attachmentCount] = {
                targets.gBuffer0.get(),
                targets.gBuffer1.get(),
                targets.gBuffer2.get(),
                targets.gBuffer3.get(),
                mirrorIndexStencil0.get(),
            };

            auto depthTarget = context.GetRenderTarget({PF_DEPTH24_STENCIL8, view.extents, true});

            GLuint fb0 = context.GetFramebuffer(attachmentCount, &attachments[0], depthTarget.get());
            attachments[attachmentCount - 1] = mirrorIndexStencil1.get();
            GLuint fb1 = context.GetFramebuffer(attachmentCount, &attachments[0], depthTarget.get());

            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);

            ecs::View forwardPassView = view;
            forwardPassView.offset = glm::ivec2();
            forwardPassView.clearMode.reset();

            glBindFramebuffer(GL_FRAMEBUFFER, fb0);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (forwardPassView.clearColor != glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) {
                glClearBufferfv(GL_COLOR, 0, glm::value_ptr(forwardPassView.clearColor));
            }

            mirrorSceneData.Bind(GL_SHADER_STORAGE_BUFFER, 1);

            int mirrorCount = 0;
            for (auto entity : lock.EntitiesWith<ecs::Mirror>()) {
                auto &mirror = entity.Get<ecs::Mirror>(lock);
                mirror.mirrorId = mirrorCount++;
            }

            GLMirrorData mirrorData[MAX_MIRRORS];
            int mirrorDataCount = FillMirrorData(&mirrorData[0], lock);

            auto sceneVS = shaders.Get<SceneVS>();
            auto sceneGS = shaders.Get<SceneGS>();
            auto sceneFS = shaders.Get<SceneFS>();

            sceneGS->SetParams(forwardPassView, {}, {});

            int recursion = mirrorCount ? std::min(MAX_MIRROR_RECURSION, CVarMirrorRecursion.Get()) : 0;

            forwardPassView.stencil = true;
            glClearStencil(~0);
            glEnable(GL_CLIP_DISTANCE0);

            for (int bounce = 0; bounce <= recursion; bounce++) {
                if (bounce > 0) {
                    RenderPhase subViewPhase("StencilCopy", timer);

                    int prevStencilBit = 1 << ((bounce - 1) % 8);
                    glStencilFunc(GL_EQUAL, 0xff, ~prevStencilBit);
                    glStencilMask(0);

                    if (bounce % 2 == 0) {
                        mirrorIndexStencil1->GetGLTexture().Bind(0);
                        SetRenderTarget(mirrorIndexStencil0.get(), nullptr);
                    } else {
                        mirrorIndexStencil0->GetGLTexture().Bind(0);
                        SetRenderTarget(mirrorIndexStencil1.get(), nullptr);
                    }

                    ShaderControl->BindPipeline<BasicPostVS, CopyStencilFS>();
                    VoxelRenderer::DrawScreenCover();
                }

                if (bounce % 2 == 0) {
                    glBindFramebuffer(GL_FRAMEBUFFER, fb0);
                    mirrorIndexStencil1->GetGLTexture().Bind(4);
                    targets.mirrorIndexStencil = mirrorIndexStencil0;
                } else {
                    glBindFramebuffer(GL_FRAMEBUFFER, fb1);
                    mirrorIndexStencil0->GetGLTexture().Bind(4);
                    targets.mirrorIndexStencil = mirrorIndexStencil1;
                }

                if (bounce == 0) {
                    forwardPassView.clearMode[ecs::View::ClearMode::CLEAR_MODE_STENCIL_BUFFER] = true;
                    sceneGS->SetRenderMirrors(false);
                } else {
                    forwardPassView.clearMode[ecs::View::ClearMode::CLEAR_MODE_STENCIL_BUFFER] = false;
                    {
                        RenderPhase subViewPhase("MatrixGen", timer);

                        shaders.Get<MirrorSceneCS>()->SetMirrorData(mirrorDataCount, &mirrorData[0]);

                        ShaderControl->BindPipeline<MirrorSceneCS>();
                        glDispatchCompute(1, 1, 1);
                        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                    }

                    {
                        RenderPhase subViewPhase("DepthClear", timer);
                        glDepthFunc(GL_ALWAYS);
                        glDisable(GL_CULL_FACE);
                        glEnable(GL_STENCIL_TEST);
                        glEnable(GL_DEPTH_TEST);
                        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                        glStencilFunc(GL_EQUAL, 0xff, 0xff);
                        glStencilMask(0);

                        ShaderControl->BindPipeline<SceneDepthClearVS, SceneDepthClearFS>();
                        VoxelRenderer::DrawScreenCover();

                        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                        glDepthFunc(GL_LESS);
                        glEnable(GL_CULL_FACE);
                    }

                    sceneGS->SetRenderMirrors(true);
                }

                int thisStencilBit = 1 << (bounce % 8);
                glStencilFunc(GL_EQUAL, 0xff, ~thisStencilBit);
                glStencilMask(~0u); // for forward pass clearMode
                glFrontFace(bounce % 2 == 0 ? GL_CCW : GL_CW);

                sceneFS->SetMirrorId(-1);

                ShaderControl->BindPipeline<SceneVS, SceneGS, SceneFS>();

                glDepthFunc(GL_LESS);
                glDepthMask(GL_FALSE);

                ForwardPass(forwardPassView, sceneVS, lock, [&](auto lock, Tecs::Entity &ent) {
                    if (bounce == recursion) {
                        // Don't mark mirrors on last pass.
                        glStencilMask(0);
                    } else if (ent && ent.Has<ecs::Mirror>(lock)) {
                        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                        glStencilMask(thisStencilBit);
                        auto &mirror = ent.Get<ecs::Mirror>(lock);
                        sceneFS->SetMirrorId(mirror.mirrorId);
                    } else {
                        glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
                        glStencilMask(thisStencilBit);
                        sceneFS->SetMirrorId(-1);
                    }

                    if (ent && ent.Has<ecs::Renderable>(lock)) {
                        auto &renderable = ent.Get<ecs::Renderable>(lock);
                        sceneFS->SetEmissive(renderable.emissive);
                    }
                });

                if (bounce == 0 && CVarShowVoxels.Get() > 0) { DrawGridDebug(view, sceneVS); }

                glDepthFunc(GL_LESS);
                glDepthMask(GL_TRUE);
            }
        }

        // Run postprocessing.
        glFrontFace(GL_CCW);
        glDepthFunc(GL_LESS);
        glDisable(GL_CLIP_DISTANCE0);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDepthMask(GL_FALSE);

        PostProcessing::Process(this, lock, view, targets);

        if (!finalOutput) { debugGuiRenderer->Render(view); }

        // AssertGLOK("Renderer::RenderFrame");
    }

    void VoxelRenderer::PrepareForView(const ecs::View &view) {
        if (view.blend) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }

        if (view.stencil) {
            glEnable(GL_STENCIL_TEST);
        } else {
            glDisable(GL_STENCIL_TEST);
        }

        glDepthMask(GL_TRUE);

        glViewport(view.offset.x, view.offset.y, view.extents.x, view.extents.y);
        glScissor(view.offset.x, view.offset.y, view.extents.x, view.extents.y);

        if (view.clearMode.any()) {
            glClearColor(view.clearColor.r, view.clearColor.g, view.clearColor.b, view.clearColor.a);
            glClear(getClearMode(view));
        }
    }

    void VoxelRenderer::ForwardPass(const ecs::View &view,
        SceneShader *shader,
        DrawLock lock,
        const PreDrawFunc &preDraw) {
        RenderPhase phase("ForwardPass", timer);
        PrepareForView(view);

        if (CVarRenderWireframe.Get()) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        for (Tecs::Entity &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (ent.Has<ecs::Renderable, ecs::Transform>(lock)) {
                if (ent.Has<ecs::Mirror>(lock)) continue;
                DrawEntity(view, shader, lock, ent, preDraw);
            }
        }

        for (Tecs::Entity &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (ent.Has<ecs::Renderable, ecs::Transform, ecs::Mirror>(lock)) {
                DrawEntity(view, shader, lock, ent, preDraw);
            }
        }

        // TODO: Move debug lines to ECS
        /*if (game->physics.IsDebugEnabled()) {
            RenderPhase phase("PhysxBounds", timer);
            MutexedVector<physx::PxDebugLine> lines = game->physics.GetDebugLines();
            DrawPhysxLines(view, shader, lines.Vector(), lock, preDraw);
        }*/

        if (CVarRenderWireframe.Get()) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    static void addLine(const ecs::View &view,
        vector<SceneVertex> &vertices,
        glm::vec3 start,
        glm::vec3 end,
        float lineWidth) {
        glm::vec3 viewPos = view.invViewMat * glm::vec4(0, 0, 0, 1);
        glm::vec3 lineDir = glm::normalize(end - start);

        glm::vec3 lineMid = glm::vec3(0.5) * (start + end);
        glm::vec3 viewDir = glm::normalize(viewPos - lineMid);

        glm::vec3 widthVec = lineWidth * glm::normalize(glm::cross(viewDir, lineDir));

        // move the positions back a bit to account for overlapping lines
        glm::vec3 pos0 = start - lineWidth * lineDir;
        glm::vec3 pos1 = end + lineWidth * lineDir;

        auto addVertex = [&](const glm::vec3 &pos) {
            vertices.push_back({{pos.x, pos.y, pos.z}, viewDir, {0, 0}});
        };

        // 2 triangles that make up a "fat" line connecting pos0 and pos1
        // with the flat face pointing at the player
        addVertex(pos0 - widthVec);
        addVertex(pos1 + widthVec);
        addVertex(pos0 + widthVec);

        addVertex(pos1 - widthVec);
        addVertex(pos1 + widthVec);
        addVertex(pos0 - widthVec);
    }

    void VoxelRenderer::DrawPhysxLines(const ecs::View &view,
        SceneShader *shader,
        const vector<physx::PxDebugLine> &lines,
        DrawLock lock,
        const PreDrawFunc &preDraw) {
        // TODO: Fix Physx lines
        /*Tecs::Entity nullEnt;
        if (preDraw) preDraw(lock, nullEnt);

        vector<SceneVertex> vertices(6 * lines.size());
        for (auto &line : lines) {
            addLine(view, vertices, PxVec3ToGlmVec3(line.pos0), PxVec3ToGlmVec3(line.pos1), 0.004f);
        }

        shader->SetParams(view, glm::mat4(), glm::mat4());

        static unsigned char baseColor[4] = {0, 0, 255, 255};
        static BasicMaterial mat(baseColor);

        static VertexBuffer vbo;
        vbo.SetElementsVAO(vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
        vbo.BindVAO();

        mat.baseColorTex.Bind(0);
        mat.metallicRoughnessTex.Bind(1);
        mat.heightTex.Bind(3);

        glDrawArrays(GL_TRIANGLES, 0, vbo.Elements());*/
    }

    void VoxelRenderer::DrawEntity(const ecs::View &view,
        SceneShader *shader,
        DrawLock lock,
        Tecs::Entity &ent,
        const PreDrawFunc &preDraw) {
        auto &comp = ent.Get<ecs::Renderable>(lock);
        if (!comp.model || !comp.model->Valid()) return;

        // Filter entities that aren't members of all layers in the view's visibility mask.
        ecs::Renderable::VisibilityMask mask = comp.visibility;
        mask &= view.visibilityMask;
        if (mask != view.visibilityMask) return;

        glm::mat4 modelMat = ent.Get<ecs::Transform>(lock).GetGlobalTransform(lock).GetTransform();

        if (preDraw) preDraw(lock, ent);

        auto model = activeModels.Load(comp.model->name);
        if (!model) {
            model = std::make_shared<GLModel>(comp.model, this);
            activeModels.Register(comp.model->name, model);
        }

        model->Draw(shader, modelMat, view, comp.model->Bones().size(), comp.model->Bones().data());
    }

    void VoxelRenderer::DrawGridDebug(const ecs::View &view, SceneShader *shader) {
        int gridSize = CVarVoxelGridSize.Get() >> (CVarShowVoxels.Get() - 1);
        glm::vec3 min = voxelContext.voxelGridCenter - (voxelContext.voxelSize * (float)(voxelContext.gridSize / 2));
        glm::vec3 max = voxelContext.voxelGridCenter + (voxelContext.voxelSize * (float)(voxelContext.gridSize / 2));

        std::vector<SceneVertex> vertices;
        for (int a = 0; a <= gridSize; a++) {
            float x = min.x + (float)a * (max.x - min.x) / (float)gridSize;
            float y = min.y + (float)a * (max.y - min.y) / (float)gridSize;
            for (int b = 0; b <= gridSize; b++) {
                float y2 = min.y + (float)b * (max.y - min.y) / (float)gridSize;
                float z = min.z + (float)b * (max.z - min.z) / (float)gridSize;

                addLine(view, vertices, glm::vec3(min.x, y, z), glm::vec3(max.x, y, z), 0.001f);
                addLine(view, vertices, glm::vec3(x, min.y, z), glm::vec3(x, max.y, z), 0.001f);
                addLine(view, vertices, glm::vec3(x, y2, min.z), glm::vec3(x, y2, max.z), 0.001f);
            }
        }

        shader->SetParams(view, glm::mat4(), glm::mat4());

        static unsigned char baseColor[4] = {0, 255, 0, 255};
        static BasicMaterial mat(baseColor);

        static VertexBuffer vbo;
        vbo.SetElementsVAO(vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
        vbo.BindVAO();

        mat.baseColorTex.Bind(0);
        mat.metallicRoughnessTex.Bind(1);
        mat.heightTex.Bind(3);

        glDrawArrays(GL_TRIANGLES, 0, vbo.Elements());
    }

    void VoxelRenderer::BeginFrame(ecs::Lock<ecs::Read<ecs::Transform>,
        ecs::Write<ecs::Renderable, ecs::View, ecs::Light, ecs::LightSensor, ecs::Mirror, ecs::VoxelArea>> lock) {
        RenderPhase phase("BeginFrame", timer);

        UpdateShaders();
        ReadBackLightSensors(lock);

        if (menuGui && menuGui->RenderMode() == MenuRenderMode::Gel) {
            ecs::View menuView({1280, 1280});
            menuView.clearMode.reset();
            menuView.clearMode[ecs::View::ClearMode::CLEAR_MODE_COLOR_BUFFER] = true;
            RenderMainMenu(menuView, true);
        }

        RenderShadowMaps(lock);

        voxelContext.UpdateCache(lock);
        if (CVarUpdateVoxels.Get()) RenderVoxelGrid(lock);
        UpdateLightSensors(lock);
    }

    void VoxelRenderer::EndFrame() {
        activeModels.Tick(std::chrono::milliseconds(33)); // Minimum 30 fps tick rate
    }

    void VoxelRenderer::SetRenderTargets(size_t attachmentCount, GLRenderTarget **attachments, GLRenderTarget *depth) {
        GLuint fb = context.GetFramebuffer(attachmentCount, attachments, depth);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
    }

    void VoxelRenderer::SetRenderTarget(GLRenderTarget *attachment0, GLRenderTarget *depth) {
        SetRenderTargets(1, &attachment0, depth);
    }

    void VoxelRenderer::SetDefaultRenderTarget() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void VoxelRenderer::DrawScreenCover(bool flipped) {
        if (flipped) {
            static TextureVertex screenCoverElements[] = {
                {{-2, -1, 0}, {-0.5, 1}},
                {{2, -1, 0}, {1.5, 1}},
                {{0, 3, 0}, {0.5, -1}},
            };

            static VertexBuffer vbo;

            if (!vbo.Initialized()) { vbo.SetElementsVAO(3, screenCoverElements); }

            vbo.BindVAO();
            glDrawArrays(GL_TRIANGLES, 0, vbo.Elements());
        } else {
            static TextureVertex screenCoverElements[] = {
                {{-2, -1, 0}, {-0.5, 0}},
                {{2, -1, 0}, {1.5, 0}},
                {{0, 3, 0}, {0.5, 2}},
            };

            static VertexBuffer vbo;

            if (!vbo.Initialized()) { vbo.SetElementsVAO(3, screenCoverElements); }

            vbo.BindVAO();
            glDrawArrays(GL_TRIANGLES, 0, vbo.Elements());
        }
    }
} // namespace sp
