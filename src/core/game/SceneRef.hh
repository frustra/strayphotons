#pragma once

#include <memory>
#include <string>

namespace sp {
    class Scene;

    struct SceneRef {
        std::string name;
        std::weak_ptr<Scene> ptr;

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
    };
} // namespace sp
