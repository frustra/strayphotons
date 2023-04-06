#include "SceneRef.hh"

#include "ecs/components/SceneProperties.hh"
#include "game/Scene.hh"

#include <memory>

namespace sp {
    SceneRef::SceneRef(const std::shared_ptr<Scene> &scene) : data(scene->data), ptr(scene) {}

    bool SceneRef::operator==(const SceneRef &other) const {
        return data && data == other.data;
    }

    bool SceneRef::operator==(const Scene &scene) const {
        return data && data->name == scene.data->name;
    }

    bool SceneRef::operator==(const std::shared_ptr<Scene> &scene) const {
        return data && scene && data->name == scene->data->name;
    }

    // Thread-safe equality check without weak_ptr::lock()
    bool SceneRef::operator==(const std::weak_ptr<Scene> &scene) const {
        return !ptr.owner_before(scene) && !scene.owner_before(ptr);
    }

    bool SceneRef::operator<(const SceneRef &other) const {
        if (!data) return !!other.data;
        if (!other.data) return false;
        return *data < *other.data;
    }

    const ecs::SceneProperties &SceneMetadata::GetProperties(ecs::Lock<ecs::Read<ecs::SceneProperties>> lock) const {
        static const ecs::SceneProperties defaultProperties = {};
        auto sceneId = sceneEntity.Get(lock);
        if (sceneId.Has<ecs::SceneProperties>(lock)) {
            return sceneId.Get<ecs::SceneProperties>(lock);
        }
        return defaultProperties;
    }

    bool SceneMetadata::operator<(const SceneMetadata &other) const {
        return type == other.type ? name < other.name : type < other.type;
    }
} // namespace sp
