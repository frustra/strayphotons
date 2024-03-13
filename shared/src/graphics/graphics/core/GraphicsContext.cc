/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GraphicsContext.hh"

#include "console/CVar.hh"

#include <glm/glm.hpp>

namespace sp {
    CVar<float> CVarFieldOfView("r.FieldOfView", 60, "Camera field of view");
    CVar<glm::ivec2> CVarWindowSize("r.Size", {1920, 1080}, "Window width and height");
    CVar<bool> CVarWindowFullscreen("r.Fullscreen", false, "Fullscreen window (0: window, 1: fullscreen)");
} // namespace sp
