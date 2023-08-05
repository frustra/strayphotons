#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(render_graph)
add_subdirectory(render_passes)
add_subdirectory(scene)

target_sources(${PROJECT_GRAPHICS_VULKAN_HEADLESS_LIB} PRIVATE
    Renderer.cc
)

target_sources(${PROJECT_GRAPHICS_VULKAN_DESKTOP_LIB} PRIVATE
    Renderer.cc
)

target_sources(${PROJECT_GRAPHICS_VULKAN_XR_LIB} PRIVATE
    Renderer.cc
)
