#include "Graphics.hh"

#include "core/Logging.hh"

namespace sp {
    void AssertGLOK(string message) {
        auto err = glGetError();
        if (err) {
            Errorf("OpenGL error %d", err);
            Abort("OpenGL error at " + message);
        }
    }
} // namespace sp
