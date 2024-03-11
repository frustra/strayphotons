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
    DeviceContext.cc
    Image.cc
    Memory.cc
    PerfTimer.cc
    Pipeline.cc
    RenderPass.cc
    UniqueID.cc
    Util.cc
    VkCommon.cc
)
