#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

target_sources(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PRIVATE
    Access.cc
    BufferPool.cc
    CommandContext.cc
    Image.cc
    Memory.cc
    PerfTimer.cc
    Pipeline.cc
    RenderPass.cc
    UniqueID.cc
    Util.cc
    VkCommon.cc
)

target_sources(${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB} PRIVATE
    DeviceContext.cc
)

target_sources(${PROJECT_GRAPHICS_VULKAN_GLFW_LIB} PRIVATE
    DeviceContext.cc
)

target_sources(${PROJECT_GRAPHICS_VULKAN_WINIT_LIB} PRIVATE
    DeviceContext.cc
)

target_link_libraries(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PUBLIC VulkanMemoryAllocator SPIRV-Reflect sp-rs)
