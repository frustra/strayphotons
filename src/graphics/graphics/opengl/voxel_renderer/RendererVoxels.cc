#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/opengl/GlfwGraphicsContext.hh"
#include "graphics/opengl/PerfTimer.hh"
#include "graphics/opengl/RenderTargetPool.hh"
#include "graphics/opengl/SceneShaders.hh"
#include "graphics/opengl/voxel_renderer/VoxelRenderer.hh"

#include <atomic>
#include <memory>

namespace sp {
    static CVar<float> CVarLightAttenuation("r.LightAttenuation", 0.5, "Light attenuation for voxel bounces");
    static CVar<float> CVarMaxVoxelFill("r.MaxVoxelFill", 0.5, "Maximum percentage of voxels that can be filled");

    std::atomic_bool printGfxDebug = false;
    static CFunc<void> CFuncList("printgfx", "Print the graphics debug output", []() {
        printGfxDebug = true;
    });

    void VoxelRenderer::PrepareVoxelTextures() {
        auto VoxelGridSize = CVarVoxelGridSize.Get();
        auto VoxelListSize = VoxelGridSize * VoxelGridSize * VoxelGridSize * CVarMaxVoxelFill.Get();

        glm::ivec3 gridDimensions = glm::ivec3(VoxelGridSize, VoxelGridSize, VoxelGridSize);
        auto VoxelMipLevels = ceil(log2(VoxelGridSize));

        // { listSize, indirect_x, indirect_y, indirect_z }
        GLsizei indirectBufferSize = sizeof(GLuint) * 4 * VoxelMipLevels;
        if (!indirectBufferCurrent) indirectBufferCurrent.Create();
        if (!indirectBufferPrevious) indirectBufferPrevious.Create();
        if (indirectBufferCurrent.size != indirectBufferSize || indirectBufferPrevious.size != indirectBufferSize) {
            indirectBufferCurrent.Data(indirectBufferSize, nullptr, GL_DYNAMIC_COPY);
            indirectBufferPrevious.Data(indirectBufferSize, nullptr, GL_DYNAMIC_COPY);

            GLuint listData[] = {0, 0, 1, 1};
            indirectBufferCurrent.Clear(PF_RGBA32UI, listData);
            indirectBufferPrevious.Clear(PF_RGBA32UI, listData);
        }

        RenderTargetDesc listDesc(PF_RGB10_A2UI, glm::ivec2(8192, ceil(VoxelListSize / 8192.0)));
        listDesc.levels = VoxelMipLevels;
        listDesc.Prepare(context, voxelContext.fragmentListCurrent);
        listDesc.Prepare(context, voxelContext.fragmentListPrevious);

        RenderTargetDesc counterDesc(PF_R32UI, gridDimensions);
        counterDesc.levels = VoxelMipLevels;
        counterDesc.Prepare(context, voxelContext.voxelCounters, true);

        RenderTargetDesc overflowDesc(PF_RGBA16F, glm::ivec2(8192, ceil(VoxelListSize / 8192.0)));
        overflowDesc.levels = VoxelMipLevels;
        overflowDesc.Prepare(context, voxelContext.voxelOverflow);

        RenderTargetDesc radianceDesc(PF_RGBA16F, gridDimensions);
        radianceDesc.Wrap(GL_CLAMP_TO_BORDER);
        radianceDesc.borderColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        radianceDesc.Prepare(context, voxelContext.radiance, true);

        auto mipSize = gridDimensions / 2;
        mipSize.x *= MAX_VOXEL_AREAS;

        RenderTargetDesc radianceMipsDesc(PF_RGBA16F, mipSize);
        radianceMipsDesc.levels = VoxelMipLevels - 1;
        radianceMipsDesc.Wrap(GL_CLAMP_TO_BORDER);
        radianceMipsDesc.borderColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        radianceMipsDesc.Prepare(context, voxelContext.radianceMips, true);
    }

    void VoxelRenderer::RenderVoxelGrid(
        ecs::Lock<ecs::Read<ecs::Renderable, ecs::Transform, ecs::View, ecs::Light>, ecs::Write<ecs::Mirror>> lock) {
        RenderPhase phase("VoxelGrid", timer);

        PrepareVoxelTextures();

        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        auto VoxelGridSize = CVarVoxelGridSize.Get();

        ecs::View ortho;
        ortho.viewType = ecs::View::VIEW_TYPE_LIGHTING;
        ortho.viewMat = glm::scale(glm::mat4(), glm::vec3(2.0 / (VoxelGridSize * voxelContext.voxelSize)));
        ortho.viewMat = glm::translate(ortho.viewMat, -voxelContext.voxelGridCenter);
        ortho.projMat = glm::mat4();
        ortho.extents = glm::ivec2(VoxelGridSize * CVarVoxelSuperSample.Get());
        ortho.clearMode.reset();

        auto renderTarget = context.GetRenderTarget({PF_R8, ortho.extents});
        SetRenderTarget(renderTarget.get(), nullptr);

        GLVoxelInfo voxelInfo;
        FillVoxelInfo(&voxelInfo, voxelContext);
        GLLightData lightData[MAX_LIGHTS];
        int lightCount = FillLightData(&lightData[0], lock);

        {
            RenderPhase phase("Fill", timer);

            indirectBufferCurrent.Bind(GL_ATOMIC_COUNTER_BUFFER, 0);
            voxelContext.voxelCounters->GetGLTexture().BindImage(0, GL_READ_WRITE, 0, GL_TRUE, 0);
            voxelContext.fragmentListCurrent->GetGLTexture().BindImage(1, GL_WRITE_ONLY, 0);
            voxelContext.radiance->GetGLTexture().BindImage(2, GL_WRITE_ONLY, 0, GL_TRUE, 0);
            voxelContext.voxelOverflow->GetGLTexture().BindImage(3, GL_WRITE_ONLY, 0);
            voxelContext.voxelOverflow->GetGLTexture().BindImage(4, GL_WRITE_ONLY, 1);
            voxelContext.voxelOverflow->GetGLTexture().BindImage(5, GL_WRITE_ONLY, 2);

            shadowMap->GetGLTexture().Bind(4);
            if (mirrorShadowMap) mirrorShadowMap->GetGLTexture().Bind(5);
            if (menuGuiTarget) menuGuiTarget->GetGLTexture().Bind(6); // TODO(xthexder): bind correct light gel
            voxelContext.radiance->GetGLTexture().Bind(7);
            voxelContext.radianceMips->GetGLTexture().Bind(8);
            mirrorVisData.Bind(GL_SHADER_STORAGE_BUFFER, 0);

            ShaderControl->BindPipeline<VoxelFillVS, VoxelFillGS, VoxelFillFS>();
            auto voxelFillFS = shaders.Get<VoxelFillFS>();
            voxelFillFS->SetLightData(lightCount, &lightData[0]);
            voxelFillFS->SetVoxelInfo(&voxelInfo);
            voxelFillFS->SetLightAttenuation(CVarLightAttenuation.Get());

            ForwardPass(ortho, shaders.Get<VoxelFillVS>(), lock);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT |
                            GL_COMMAND_BARRIER_BIT);
        }

        {
            RenderPhase phase("Merge", timer);

            indirectBufferCurrent.Bind(GL_DISPATCH_INDIRECT_BUFFER);

            // TODO(xthexder): Make last bucket sequencial to eliminate flickering
            for (uint32 i = 0; i < 3; i++) {
                indirectBufferCurrent.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * (i + 1), sizeof(GLuint));
                voxelContext.radiance->GetGLTexture().BindImage(0, GL_READ_WRITE, 0, GL_TRUE, 0);
                voxelContext.voxelOverflow->GetGLTexture().BindImage(1, GL_READ_ONLY, i);

                ShaderControl->BindPipeline<VoxelMergeCS>();
                shaders.Get<VoxelMergeCS>()->SetLevel(i);
                glDispatchComputeIndirect(sizeof(GLuint) * 4 * (i + 1) + sizeof(GLuint));
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            }

            GLuint listData[] = {0, 0, 1, 1};
            indirectBufferCurrent.ClearRegion(PF_RGBA32UI, sizeof(GLuint) * 4, -1, listData);
            glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
        }

        {
            RenderPhase phase("Mipmap", timer);

            for (uint32 i = 0; i <= voxelContext.radianceMips->GetDesc().levels; i++) {
                {
                    RenderPhase subPhase("Clear", timer);

                    indirectBufferPrevious.Bind(GL_DISPATCH_INDIRECT_BUFFER);
                    indirectBufferPrevious.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * i, sizeof(GLuint));
                    voxelContext.fragmentListPrevious->GetGLTexture().BindImage(0, GL_READ_ONLY, i);
                    voxelContext.voxelCounters->GetGLTexture().BindImage(1, GL_READ_ONLY, i);
                    if (i == 0) {
                        voxelContext.radiance->GetGLTexture().BindImage(2, GL_WRITE_ONLY, 0, GL_TRUE, 0);
                    } else {
                        voxelContext.radianceMips->GetGLTexture().BindImage(2, GL_WRITE_ONLY, i - 1, GL_TRUE, 0);
                    }

                    ShaderControl->BindPipeline<VoxelClearCS>();
                    shaders.Get<VoxelClearCS>()->SetLevel(i);
                    glDispatchComputeIndirect(sizeof(GLuint) * 4 * i + sizeof(GLuint));
                    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                }

                {
                    RenderPhase subPhase("MipmapLevel", timer);

                    indirectBufferCurrent.Bind(GL_DISPATCH_INDIRECT_BUFFER);
                    indirectBufferCurrent.Bind(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * 4 * i, sizeof(GLuint) * 4);
                    voxelContext.fragmentListCurrent->GetGLTexture().BindImage(0, GL_READ_ONLY, i);
                    voxelContext.voxelCounters->GetGLTexture().BindImage(2, GL_WRITE_ONLY, i);
                    if (i < voxelContext.radianceMips->GetDesc().levels) {
                        indirectBufferCurrent.Bind(GL_ATOMIC_COUNTER_BUFFER,
                                                   1,
                                                   sizeof(GLuint) * 4 * (i + 1),
                                                   sizeof(GLuint) * 4);
                        voxelContext.voxelCounters->GetGLTexture().BindImage(3, GL_READ_WRITE, i + 1);
                        voxelContext.fragmentListCurrent->GetGLTexture().BindImage(1, GL_WRITE_ONLY, i + 1);
                    }
                    if (i > 0) {
                        if (i > 1)
                            voxelContext.radianceMips->GetGLTexture().BindImage(4, GL_READ_ONLY, i - 2, GL_TRUE, 0);
                        else
                            voxelContext.radiance->GetGLTexture().BindImage(4, GL_READ_ONLY, 0, GL_TRUE, 0);
                        voxelContext.radianceMips->GetGLTexture().BindImage(5, GL_WRITE_ONLY, i - 1, GL_TRUE, 0);
                    }

                    ShaderControl->BindPipeline<VoxelMipmapCS>();
                    auto voxelMipmapCS = shaders.Get<VoxelMipmapCS>();
                    voxelMipmapCS->SetVoxelInfo(&voxelInfo);
                    voxelMipmapCS->SetLevel(i);

                    glDispatchComputeIndirect(sizeof(GLuint) * 4 * i + sizeof(GLuint));
                    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT |
                                    GL_COMMAND_BARRIER_BIT);
                }
            }
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
        }

        if (printGfxDebug) {
            uint32 *buf = (uint32 *)indirectBufferCurrent.Map(GL_READ_ONLY);
            Logf("Size: %d %d %d %d", buf[0], buf[4], buf[8], buf[12]);
            Logf("Compute count: %d %d %d %d", buf[1], buf[5], buf[9], buf[13]);
            indirectBufferCurrent.Unmap();

            indirectBufferCurrent.Delete();
            PrepareVoxelTextures();

            printGfxDebug = false;
        }

        {
            RenderPhase phase("Swap", timer);

            GLBuffer tmp = indirectBufferPrevious;
            indirectBufferPrevious = indirectBufferCurrent;
            indirectBufferCurrent = tmp;

            std::shared_ptr<GLRenderTarget> tmp2 = voxelContext.fragmentListPrevious;
            voxelContext.fragmentListPrevious = voxelContext.fragmentListCurrent;
            voxelContext.fragmentListCurrent = tmp2;

            GLuint listData[] = {0, 0, 1, 1};
            indirectBufferCurrent.Clear(PF_RGBA32UI, listData);
        }

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }
} // namespace sp
