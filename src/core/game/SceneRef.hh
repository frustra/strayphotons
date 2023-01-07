#pragma once

#include <memory>
#include <string>

namespace ecs {
    struct Scripts;
}

namespace sp {
    class Scene;
    class SceneManager;
    struct SceneProperties;

    enum class SceneType {
        Async = 0,
        World,
        System,
    };

    class SceneRef {
    public:
        SceneRef() {}
        SceneRef(const std::shared_ptr<Scene> &scene);

        explicit operator bool() const {
            return !name.empty();
        }

        bool operator==(const SceneRef &other) const;
        bool operator==(const Scene &scene) const;
        bool operator==(const std::shared_ptr<Scene> &scene) const;
        // Thread-safe equality check without weak_ptr::lock()
        bool operator==(const std::weak_ptr<Scene> &scene) const;
        bool operator<(const SceneRef &other) const;

        std::string name;
        SceneType type;
        std::shared_ptr<SceneProperties> properties;

        // Must only be called by SceneManager.
        // Defined in game/SceneManager.cc
        std::shared_ptr<Scene> Lock() const;

    private:
        std::weak_ptr<Scene> ptr;
    };
} // namespace sp
