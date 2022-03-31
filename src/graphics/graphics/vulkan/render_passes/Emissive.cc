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

            LaserLine drawLine;
            drawLine.radius = laser.radius;
            drawLine.mediaDensityFactor = laser.mediaDensityFactor;

            auto line = std::get_if<ecs::LaserLine::Line>(&laser.line);
            auto segments = std::get_if<ecs::LaserLine::Segments>(&laser.line);
            if (line && line->points.size() >= 2) {
                drawLine.color = line->color * laser.intensity;
                drawLine.start = transform * glm::vec4(line->points[0], 1);

                for (size_t i = 1; i < line->points.size(); i++) {
                    drawLine.end = transform * glm::vec4(line->points[i], 1);
                    lasers.push_back(drawLine);
                    drawLine.start = drawLine.end;
                }
            } else if (segments && segments->size() > 0) {
                for (auto &segment : *segments) {
                    drawLine.start = transform * glm::vec4(segment.start, 1);
                    drawLine.end = transform * glm::vec4(segment.end, 1);
                    drawLine.color = segment.color * laser.intensity;
                    lasers.push_back(drawLine);
                }
            }
        }

        graph.AddPass("Emissive")
            .Build([&](PassBuilder &builder) {
                builder.Read("GBuffer0", Access::FragmentShaderSampleImage);
                builder.Read("GBuffer1", Access::FragmentShaderSampleImage);
                builder.Read("ExposureState", Access::FragmentShaderReadStorage);
                builder.ReadUniform("ViewState");

                builder.SetColorAttachment(0, builder.LastOutputID(), {LoadOp::Load, StoreOp::Store});
                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::Store});

                for (auto ent : lock.EntitiesWith<ecs::Screen>()) {
                    if (!ent.Has<ecs::Transform>(lock)) continue;

                    auto &screenComp = ent.Get<ecs::Screen>(lock);
                    auto id = builder.Read(screenComp.textureName, Access::FragmentShaderSampleImage);

                    Screen screen;
                    screen.id = id;
                    screen.gpuData.luminanceScale = screenComp.luminanceScale;
                    screen.gpuData.quad = ent.Get<ecs::TransformSnapshot>(lock).matrix;
                    screens.push_back(std::move(screen));
                }
            })
            .Execute([this](Resources &resources, CommandContext &cmd) {
                cmd.SetStencilTest(true);
                cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                cmd.SetDepthTest(true, false);
                cmd.SetDepthCompareOp(vk::CompareOp::eLessOrEqual);
                cmd.SetCullMode(vk::CullModeFlagBits::eNone);
                cmd.SetBlending(true);
                cmd.SetBlendFunc(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOne);
                cmd.SetPrimitiveTopology(vk::PrimitiveTopology::eTriangleStrip);
                cmd.SetUniformBuffer(0, 0, resources.GetBuffer("ViewState"));
                cmd.SetStorageBuffer(0, 1, resources.GetBuffer("ExposureState"));

                {
                    RenderPhase phase("LaserLines");
                    phase.StartTimer(cmd);
                    cmd.SetShaders("laser_billboard.vert", "laser_billboard.frag");

                    struct {
                        glm::vec3 radiance;
                        float radius;
                        glm::vec3 start;
                        float mediaDensityFactor;
                        glm::vec3 end;
                        float time;
                    } constants;

                    static chrono_clock::time_point epoch = chrono_clock::now();
                    constants.time =
                        std::chrono::duration_cast<std::chrono::milliseconds>(chrono_clock::now() - epoch).count() /
                        1000.0f;

                    for (auto &line : lasers) {
                        constants.radiance = line.color;
                        constants.radius = line.radius;
                        constants.start = line.start;
                        constants.end = line.end;
                        constants.mediaDensityFactor = line.mediaDensityFactor;
                        cmd.PushConstants(constants);
                        cmd.Draw(4);
                    }
                }

                {
                    RenderPhase phase("LaserContactPoints");
                    phase.StartTimer(cmd);
                    cmd.SetShaders("laser_contact.vert", "laser_contact.frag");
                    cmd.SetImageView(0, 2, resources.GetImageView("GBuffer0"));
                    cmd.SetImageView(0, 3, resources.GetImageView("GBuffer1"));

                    struct {
                        glm::vec3 radiance;
                        float radius;
                        glm::vec3 point;
                    } constants;

                    for (auto &line : lasers) {
                        constants.radiance = line.color;
                        constants.radius = line.radius;
                        constants.point = line.end;
                        cmd.PushConstants(constants);
                        cmd.Draw(4);
                    }
                }

                {
                    RenderPhase phase("Screens");
                    phase.StartTimer(cmd);
                    cmd.SetShaders("textured_quad.vert", "single_texture.frag");
                    cmd.SetUniformBuffer(0, 1, resources.GetBuffer("ViewState"));

                    for (auto &screen : screens) {
                        cmd.SetImageView(0, 0, resources.GetImageView(screen.id));
                        cmd.PushConstants(screen.gpuData);
                        cmd.Draw(4);
                    }
                }
            });
    }
} // namespace sp::vulkan::renderer
