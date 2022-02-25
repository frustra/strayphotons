#include "Emissive.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/PerfTimer.hh"

namespace sp::vulkan::renderer {
    void Emissive::AddPass(RenderGraph &graph,
        ecs::Lock<ecs::Read<ecs::Screen, ecs::LaserLine, ecs::TransformSnapshot>> lock) {
        screens.clear();

        lasers.gpuData.resize(0);
        lasers.gpuData.reserve(1);
        for (auto entity : lock.EntitiesWith<ecs::LaserLine>()) {
            auto &laser = entity.Get<ecs::LaserLine>(lock);

            glm::mat4x3 transform;
            if (laser.relative && entity.Has<ecs::TransformSnapshot>(lock)) {
                transform = entity.Get<ecs::TransformSnapshot>(lock).matrix;
            }

            if (!laser.on) continue;
            if (laser.points.size() < 2) continue;

            LaserContext::GPULine line;
            line.color = laser.color * laser.intensity;
            line.start = transform * glm::vec4(laser.points[0], 1);

            for (size_t i = 1; i < laser.points.size(); i++) {
                line.end = transform * glm::vec4(laser.points[i], 1);
                lasers.gpuData.push_back(line);
                line.start = line.end;
            }
        }

        graph.AddPass("Emissive")
            .Build([&](PassBuilder &builder) {
                auto input = builder.LastOutput();
                builder.ShaderRead(input.id);
                builder.ShaderRead("GBuffer2");

                auto desc = input.DeriveRenderTarget();
                builder.OutputColorAttachment(0, "LaserLines", desc, {LoadOp::DontCare, StoreOp::Store});

                builder.ReadBuffer("ViewState");

                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::Store});

                for (auto ent : lock.EntitiesWith<ecs::Screen>()) {
                    if (!ent.Has<ecs::Transform>(lock)) continue;

                    auto &screenComp = ent.Get<ecs::Screen>(lock);
                    auto &resource = builder.ShaderRead(screenComp.textureName);

                    Screen screen;
                    screen.id = resource.id;
                    screen.gpuData.luminanceScale = screenComp.luminanceScale;
                    screen.gpuData.quad = ent.Get<ecs::TransformSnapshot>(lock).matrix;
                    screens.push_back(std::move(screen));
                }
            })
            .Execute([this](rg::Resources &resources, CommandContext &cmd) {
                {
                    RenderPhase phase("Lasers");
                    phase.StartTimer(cmd);

                    auto laserState = cmd.Device().GetFramePooledBuffer(BUFFER_TYPE_STORAGE_TRANSFER,
                        lasers.gpuData.capacity() * sizeof(lasers.gpuData.front()));

                    laserState->CopyFrom(lasers.gpuData.data(), lasers.gpuData.size());

                    cmd.SetShaders("screen_cover.vert", "laser_lines.frag");
                    cmd.SetStencilTest(true);
                    cmd.SetDepthTest(false, false);
                    cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                    cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                    cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                    cmd.SetTexture(0, 0, resources.GetRenderTarget(resources.LastOutputID())->ImageView());
                    cmd.SetTexture(0, 1, resources.GetRenderTarget("GBuffer2")->ImageView());

                    cmd.SetUniformBuffer(0, 10, resources.GetBuffer("ViewState"));
                    cmd.SetStorageBuffer(0, 9, laserState);

                    struct {
                        uint32 laserCount;
                        float time;
                    } constants;
                    constants.laserCount = lasers.gpuData.size();
                    static chrono_clock::time_point epoch = chrono_clock::now();
                    constants.time =
                        std::chrono::duration_cast<std::chrono::milliseconds>(chrono_clock::now() - epoch).count() /
                        1000.0f;
                    cmd.PushConstants(constants);
                    cmd.Draw(3);
                }

                {
                    RenderPhase phase("Screens");
                    phase.StartTimer(cmd);
                    cmd.SetShaders("textured_quad.vert", "single_texture.frag");
                    cmd.SetDepthTest(true, false);
                    cmd.SetPrimitiveTopology(vk::PrimitiveTopology::eTriangleStrip);
                    cmd.SetCullMode(vk::CullModeFlagBits::eNone);
                    cmd.SetBlending(true);

                    for (auto &screen : screens) {
                        cmd.SetTexture(0, 0, resources.GetRenderTarget(screen.id)->ImageView());
                        cmd.PushConstants(screen.gpuData);
                        cmd.Draw(4);
                    }
                }
            });
    }
} // namespace sp::vulkan::renderer
