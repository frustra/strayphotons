#include "Emissive.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/PerfTimer.hh"

namespace sp::vulkan::renderer {
    void Emissive::AddPass(RenderGraph &graph,
        ecs::Lock<ecs::Read<ecs::Screen, ecs::LaserLine, ecs::TransformSnapshot>> lock) {
        screens.clear();

        lasers.resize(0);
        for (auto entity : lock.EntitiesWith<ecs::LaserLine>()) {
            auto &laser = entity.Get<ecs::LaserLine>(lock);

            glm::mat4x3 transform;
            if (laser.relative && entity.Has<ecs::TransformSnapshot>(lock)) {
                transform = entity.Get<ecs::TransformSnapshot>(lock).matrix;
            }

            if (!laser.on) continue;
            if (laser.points.size() < 2) continue;

            LaserLine line;
            line.color = laser.color * laser.intensity;
            line.radius = laser.radius;
            line.start = transform * glm::vec4(laser.points[0], 1);

            for (size_t i = 1; i < laser.points.size(); i++) {
                line.end = transform * glm::vec4(laser.points[i], 1);
                lasers.push_back(line);
                line.start = line.end;
            }
        }

        graph.AddPass("Emissive")
            .Build([&](PassBuilder &builder) {
                auto input = builder.LastOutput();
                builder.ShaderRead(input.id);
                builder.ShaderRead("GBuffer2");

                auto desc = input.DeriveRenderTarget();
                builder.OutputColorAttachment(0, "Emissive", desc, {LoadOp::DontCare, StoreOp::Store});

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
                cmd.SetStencilTest(true);
                cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetDepthTest(false, false);
                cmd.DrawScreenCover(resources.GetRenderTarget(resources.LastOutputID())->ImageView());

                cmd.SetDepthTest(true, false);
                cmd.SetUniformBuffer(0, 10, resources.GetBuffer("ViewState"));
                cmd.SetCullMode(vk::CullModeFlagBits::eNone);
                cmd.SetBlending(true);
                cmd.SetPrimitiveTopology(vk::PrimitiveTopology::eTriangleStrip);

                {
                    RenderPhase phase("Lasers");
                    phase.StartTimer(cmd);
                    cmd.SetShaders("laser_billboard.vert", "laser_billboard.frag");

                    struct {
                        glm::vec3 color;
                        float radius;
                        glm::vec3 start;
                        float _padding0[1];
                        glm::vec3 end;
                        float time;
                    } constants;

                    static chrono_clock::time_point epoch = chrono_clock::now();
                    constants.time =
                        std::chrono::duration_cast<std::chrono::milliseconds>(chrono_clock::now() - epoch).count() /
                        1000.0f;

                    for (auto &line : lasers) {
                        constants.color = line.color;
                        constants.radius = line.radius;
                        constants.start = line.start;
                        constants.end = line.end;
                        cmd.PushConstants(constants);
                        cmd.Draw(4);
                    }
                }

                {
                    RenderPhase phase("Screens");
                    phase.StartTimer(cmd);
                    cmd.SetShaders("textured_quad.vert", "single_texture.frag");

                    for (auto &screen : screens) {
                        cmd.SetTexture(0, 0, resources.GetRenderTarget(screen.id)->ImageView());
                        cmd.PushConstants(screen.gpuData);
                        cmd.Draw(4);
                    }
                }
            });
    }
} // namespace sp::vulkan::renderer
