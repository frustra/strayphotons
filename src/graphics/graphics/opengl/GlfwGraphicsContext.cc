#include "GlfwGraphicsContext.hh"

#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/opengl/GLTexture.hh"
#include "graphics/opengl/Graphics.hh"
#include "graphics/opengl/PerfTimer.hh"
#include "graphics/opengl/Shader.hh"
#include "graphics/opengl/ShaderManager.hh"

#include <algorithm>
#include <iostream>
#include <string>

// GLFW must be included after glew.h (Graphics.hh)
#include <GLFW/glfw3.h>
#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <glfw/glfw3native.h>
#endif

namespace sp {

    static void GLAPIENTRY DebugCallback(GLenum source,
                                         GLenum type,
                                         GLuint id,
                                         GLenum severity,
                                         GLsizei length,
                                         const GLchar *message,
                                         const void *user) {
        if (type == GL_DEBUG_TYPE_OTHER) return;
        Debugf("[GL 0x%X] 0x%X: %s", id, type, message);
    }

    static void glfwErrorCallback(int error, const char *message) {
        Errorf("GLFW returned %d: %s", error, message);
    }

    GlfwGraphicsContext::GlfwGraphicsContext() {
        glfwSetErrorCallback(glfwErrorCallback);

        if (!glfwInit()) { throw "glfw failed"; }

        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

        // Create window and surface
        auto initialSize = CVarWindowSize.Get();
        window = glfwCreateWindow(initialSize.x, initialSize.y, "STRAY PHOTONS", nullptr, nullptr);
        Assert(window, "glfw window creation failed");

        glfwMakeContextCurrent(window);

        glfwSwapInterval(0);

        glewExperimental = GL_TRUE;
        Assert(glewInit() == GLEW_OK, "glewInit failed");
        glGetError();

        Logf("OpenGL version: %s", glGetString(GL_VERSION));

        string vendorStr = (char *)glGetString(GL_VENDOR);
        if (starts_with(vendorStr, "NVIDIA")) {
            Logf("GPU vendor: NVIDIA");
            ShaderManager::SetDefine("NVIDIA_GPU");
        } else if (starts_with(vendorStr, "ATI")) {
            Logf("GPU vendor: AMD");
            ShaderManager::SetDefine("AMD_GPU");
        } else if (starts_with(vendorStr, "Intel")) {
            Logf("GPU vendor: Intel");
            ShaderManager::SetDefine("INTEL_GPU");
        } else {
            Logf("GPU vendor: Unknown (%s)", vendorStr);
            ShaderManager::SetDefine("UNKNOWN_GPU");
        }

        if (GLEW_KHR_debug) {
            glDebugMessageCallback(DebugCallback, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }

        float maxAnisotropy;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
        Debugf("Maximum anisotropy: %f", maxAnisotropy);
    }

    GlfwGraphicsContext::~GlfwGraphicsContext() {
        if (window) { glfwDestroyWindow(window); }

        glfwTerminate();
    }

    void GlfwGraphicsContext::SetTitle(string title) {
        glfwSetWindowTitle(window, title.c_str());
    }

    bool GlfwGraphicsContext::ShouldClose() {
        return !!glfwWindowShouldClose(window);
    }

    void GlfwGraphicsContext::PrepareWindowView(ecs::View &view) {
        glm::ivec2 scaled = glm::vec2(CVarWindowSize.Get()) * CVarWindowScale.Get();

        if (glfwFullscreen != CVarWindowFullscreen.Get()) {
            if (CVarWindowFullscreen.Get() == 0) {
                glfwSetWindowMonitor(window, nullptr, storedWindowPos.x, storedWindowPos.y, scaled.x, scaled.y, 0);
                glfwFullscreen = 0;
            } else if (CVarWindowFullscreen.Get() == 1) {
                glfwGetWindowPos(window, &storedWindowPos.x, &storedWindowPos.y);
                glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, scaled.x, scaled.y, 60);
                glfwFullscreen = 1;
            }
        } else if (glfwWindowSize != scaled) {
            if (CVarWindowFullscreen.Get()) {
                glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, scaled.x, scaled.y, 60);
            } else {
                glfwSetWindowSize(window, scaled.x, scaled.y);
            }

            glfwWindowSize = scaled;
        }

        view.extents = CVarWindowSize.Get();
        view.fov = glm::radians(CVarFieldOfView.Get());
    }

    const vector<glm::ivec2> &GlfwGraphicsContext::MonitorModes() {
        if (!monitorModes.empty()) return monitorModes;

        int count;
        const GLFWvidmode *modes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &count);

        for (int i = 0; i < count; i++) {
            glm::ivec2 size(modes[i].width, modes[i].height);
            if (std::find(monitorModes.begin(), monitorModes.end(), size) == monitorModes.end()) {
                monitorModes.push_back(size);
            }
        }

        std::sort(monitorModes.begin(), monitorModes.end(), [](const glm::ivec2 &a, const glm::ivec2 &b) {
            return a.x > b.x || (a.x == b.x && a.y > b.y);
        });

        return monitorModes;
    }

    const glm::ivec2 GlfwGraphicsContext::CurrentMode() {
        const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (mode != NULL) { return glm::ivec2(mode->width, mode->height); }
        return glm::ivec2(0);
    }

    void GlfwGraphicsContext::SwapBuffers() {
        glfwSwapBuffers(window);
    }

    void GlfwGraphicsContext::BeginFrame() {
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::FocusLock>>();
        if (lock.Has<ecs::FocusLock>()) {
            auto layer = lock.Get<ecs::FocusLock>().PrimaryFocus();
            if (layer == ecs::FocusLayer::GAME) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
    }

    void GlfwGraphicsContext::EndFrame() {
        rtPool.TickFrame();

        double frameEnd = glfwGetTime();
        fpsTimer += frameEnd - lastFrameEnd;
        frameCounter++;

        if (fpsTimer > 1.0) {
            SetTitle("STRAY PHOTONS (" + std::to_string(frameCounter) + " FPS)");
            frameCounter = 0;
            fpsTimer = 0;
        }

        lastFrameEnd = frameEnd;
    }

    void GlfwGraphicsContext::DisableCursor() {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void GlfwGraphicsContext::EnableCursor() {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    shared_ptr<GpuTexture> GlfwGraphicsContext::LoadTexture(shared_ptr<Image> image, bool genMipmap) {
        auto tex = make_shared<GLTexture>();
        tex->Create().LoadFromImage(image, genMipmap ? GLTexture::FullyMipmap : 1);
        return tex;
    }

    void *GlfwGraphicsContext::Win32WindowHandle() {
#ifdef _WIN32
        return glfwGetWin32Window(GetWindow());
#else
        return nullptr;
#endif
    }
} // namespace sp
