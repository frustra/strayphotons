#pragma once

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/SceneInfo.hh"
#include "game/SceneType.hh"

namespace sp {
    class Asset;

    class Scene : public NonCopyable {
    public:
        Scene() : type(SceneType::Count) {}
        Scene(const string &name, SceneType type) : name(name), type(type) {}
        Scene(const string &name, SceneType type, shared_ptr<const Asset> asset)
            : name(name), type(type), asset(asset) {}
        ~Scene();

        const std::string name;
        const SceneType type;

        ecs::Entity NewSystemEntity(ecs::Lock<ecs::AddRemove> stagingLock,
            const std::shared_ptr<Scene> &scene,
            ecs::Name name = ecs::Name());
        ecs::Entity NewPrefabEntity(ecs::Lock<ecs::AddRemove> stagingLock,
            ecs::Entity prefabRoot,
            ecs::Name name = ecs::Name());

        void ApplyScene(ecs::Lock<ecs::ReadAll, ecs::Write<ecs::SceneInfo>> staging, ecs::Lock<ecs::AddRemove> live);
        void RemoveScene(ecs::Lock<ecs::AddRemove> staging, ecs::Lock<ecs::AddRemove> live);

        ecs::Entity GetStagingEntity(const ecs::Name &entityName) const {
            auto it = namedEntities.find(entityName);
            if (it != namedEntities.end()) return it->second;
            return {};
        }

        ecs::Entity GetStagingEntity(const std::string &fullName, ecs::Name scope) const {
            if (scope.scene.empty()) scope.scene = this->name;
            ecs::Name entityName;
            if (entityName.Parse(fullName, scope)) {
                return GetStagingEntity(entityName);
            } else {
                Errorf("Invalid entity name: %s", fullName);
            }
            return {};
        }

    private:
        std::shared_ptr<const Asset> asset;
        bool active = false;

        std::unordered_map<ecs::Name, ecs::Entity> namedEntities;

        friend class SceneManager;
    };
} // namespace sp
