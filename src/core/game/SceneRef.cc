#include "SceneRef.hh"

#include "game/Scene.hh"

namespace sp {
    SceneRef::SceneRef(const std::shared_ptr<Scene> &scene) : name(scene->name), ptr(scene) {}

    bool SceneRef::operator==(const SceneRef &other) const {
        return !name.empty() && name == other.name;
    }

    bool SceneRef::operator==(const Scene &scene) const {
        return !name.empty() && name == scene.name;
    }

    bool SceneRef::operator==(const std::shared_ptr<Scene> &scene) const {
        return !name.empty() && scene && name == scene->name;
    }

    // Thread-safe equality check without weak_ptr::lock()
    bool SceneRef::operator==(const std::weak_ptr<Scene> &scene) const {
        return !ptr.owner_before(scene) && !scene.owner_before(ptr);
    }

    bool SceneRef::operator<(const SceneRef &other) const {
        return name < other.name;
    }
} // namespace sp
