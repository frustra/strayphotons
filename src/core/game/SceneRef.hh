#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"

#include <memory>
#include <string>

namespace sp {
    class Scene;
    class SceneManager;

    enum class SceneType {
        Async = 0,
        World,
        System,
    };

    // Lower priority scenes will have their components overwritten with higher priority components.
    enum class ScenePriority {
        System, // Lowest priority
        Scene,
        Player,
        Bindings,
        Override, // Highest priority
    };

    struct SceneMetadata {
        std::string name;
        SceneType type;
        ScenePriority priority;
        ecs::EntityRef sceneEntity;

        const ecs::SceneProperties &GetProperties(ecs::Lock<ecs::Read<ecs::SceneProperties>> lock) const;

        SceneMetadata(const string &name, SceneType type, ScenePriority priority, ecs::Entity sceneId)
            : name(name), type(type), priority(priority), sceneEntity(ecs::Name("scene", name), sceneId) {}

        bool operator<(const SceneMetadata &other) const;
    };

    class SceneRef {
    public:
        SceneRef() {}
        SceneRef(const std::shared_ptr<Scene> &scene);

        operator bool() const {
            return !!data;
        }

        bool operator==(const SceneRef &other) const;
        bool operator==(const Scene &scene) const;
        bool operator==(const std::shared_ptr<Scene> &scene) const;
        // Thread-safe equality check without weak_ptr::lock()
        bool operator==(const std::weak_ptr<Scene> &scene) const;
        bool operator<(const SceneRef &other) const;

        std::shared_ptr<const SceneMetadata> data;

        // Must only be called by SceneManager.
        // Defined in game/SceneManager.cc
        std::shared_ptr<Scene> Lock() const;

    private:
        std::weak_ptr<Scene> ptr;
    };
} // namespace sp
