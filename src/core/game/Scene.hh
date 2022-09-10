#pragma once

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/SceneInfo.hh"

namespace sp {
    class Asset;

    enum class SceneType {
        Async = 0,
        World,
        System,
        Count,
    };

    class Scene : public NonCopyable {
    public:
        Scene() : type(SceneType::Count) {}
        Scene(const string &name, SceneType type) : name(name), type(type) {}
        Scene(const string &name, SceneType type, shared_ptr<const Asset> asset)
            : name(name), type(type), asset(asset) {}
        ~Scene();

        const std::string name;
        const SceneType type;

        ecs::Entity GetStagingEntity(const ecs::Name &entityName) const {
            auto it = namedEntities.find(entityName);
            if (it != namedEntities.end()) return it->second;
            return {};
        }

        // Defined in game module: game/game/Scene.cc
        ecs::Entity NewSystemEntity(ecs::Lock<ecs::AddRemove> stagingLock,
            const std::shared_ptr<Scene> &scene,
            ecs::Name name = ecs::Name());
        ecs::Entity NewRootEntity(ecs::Lock<ecs::AddRemove> stagingLock,
            const std::shared_ptr<Scene> &scene,
            ecs::SceneInfo::Priority priority,
            std::string relativeName = "");
        ecs::Entity NewPrefabEntity(ecs::Lock<ecs::AddRemove> stagingLock,
            ecs::Entity prefabRoot,
            std::string relativeName = "",
            ecs::Name prefix = ecs::Name());

        void ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging, ecs::Lock<ecs::AddRemove> live);
        void RemoveScene(ecs::Lock<ecs::AddRemove> staging, ecs::Lock<ecs::AddRemove> live);

    private:
        ecs::Name GenerateEntityName(const ecs::Name &prefix);
        bool ValidateEntityName(const ecs::Name &name) const;

        std::shared_ptr<const Asset> asset;
        bool active = false;
        size_t unnamedEntityCount = 0;

        std::unordered_map<ecs::Name, ecs::Entity> namedEntities;
        std::vector<ecs::EntityRef> references;

        friend class SceneManager;
    };
} // namespace sp
