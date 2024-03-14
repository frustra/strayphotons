#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

target_sources(${PROJECT_GRAPHICS_VULKAN_CORE_LIB} PRIVATE
    Bloom.cc
    Blur.cc
    Crosshair.cc
    Emissive.cc
    Exposure.cc
    Mipmap.cc
    Screenshots.cc
    SMAA.cc
    Lighting.cc
    LightSensors.cc
    Outline.cc
    Tonemap.cc
    Transparency.cc
    VisualizeBuffer.cc
    Voxels.cc
)
