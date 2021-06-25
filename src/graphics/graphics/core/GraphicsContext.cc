#include "GraphicsContext.hh"

#include "core/CVar.hh"

#include <glm/glm.hpp>

namespace sp {
    CVar<glm::ivec2> CVarWindowSize("r.Size", {1280, 720}, "Window height");
    CVar<float> CVarWindowScale("r.Scale", 1.0f, "Scale framebuffer");
    CVar<float> CVarFieldOfView("r.FieldOfView", 60, "Camera field of view");
    CVar<int> CVarWindowFullscreen("r.Fullscreen", false, "Fullscreen window (0: window, 1: fullscreen)");
} // namespace sp
