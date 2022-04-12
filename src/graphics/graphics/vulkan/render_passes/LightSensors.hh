#pragma once

#include "Common.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

#include <optional>
#include <string>

namespace sp::vulkan::renderer {
    void AddLightSensors(RenderGraph &graph, GPUScene &scene, ecs::Lock<ecs::Read<ecs::LightSensor, ecs::TransformSnapshot>> lock);
} // namespace sp::vulkan::renderer
