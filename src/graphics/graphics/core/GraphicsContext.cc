/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GraphicsContext.hh"

#include "console/CVar.hh"

#include <glm/glm.hpp>

namespace sp {
    CVar<float> CVarFieldOfView("r.FieldOfView", 60, "Camera field of view");
    CVar<glm::ivec2> CVarWindowSize("r.WindowSize", {1920, 1080}, "Window width and height");
    CVar<glm::vec2> CVarWindowScale("r.WindowScale", {0, 0}, "Window scaling factor (0: auto)");
    CVar<bool> CVarWindowFullscreen("r.Fullscreen", false, "Fullscreen window (0: window, 1: fullscreen)");
    CVar<SwapchainColorSpace> CVarHDR("r.HDR", SwapchainColorSpace::SDR, "Change swapchain surface color space");
    CVar<float> CVarHDRExposreScale("r.HDRExposeScale", 4.0f, "The multiplier for exposure when HDR is enabled");
    CVar<glm::vec3> CVarWhitePoint("r.TonemapWhitePoint", glm::vec3(1), "The target whitepoint for tonemapping.");
} // namespace sp
