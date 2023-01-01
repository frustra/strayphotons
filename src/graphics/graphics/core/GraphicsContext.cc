#include "GraphicsContext.hh"

#include "console/CVar.hh"

#include <glm/glm.hpp>

namespace sp {
    CVar<float> CVarFieldOfView("r.FieldOfView", 60, "Camera field of view");
    CVar<glm::ivec2> CVarWindowSize("r.Size", {1920, 1080}, "Window height");
    CVar<bool> CVarWindowFullscreen("r.Fullscreen", false, "Fullscreen window (0: window, 1: fullscreen)");
} // namespace sp
