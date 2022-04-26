#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <picojson/picojson.h>

namespace ecs {
    InternalPrefab templatePrefab("template", [](ScriptState &state, Lock<AddRemove> lock, Entity ent) {
        auto scene = state.scope.scene.lock();
        Assertf(scene, "Template prefab does not have a valid scene: %s", ToString(lock, ent));

        auto sourceName = state.GetParam<std::string>("source");

        ecs::Name scope(scene->name, "");
        if (ent.Has<Name>(lock)) scope = ent.Get<Name>(lock);
        Debugf("Loading template: %s with scope '%s'", sourceName, scope.String());

        auto asset = sp::GAssets.Load("scenes/templates/" + sourceName + ".json", sp::AssetType::Bundled, true)->Get();
        if (!asset) {
            Errorf("Template not found: %s", sourceName);
            return;
        }

        picojson::value root;
        string err = picojson::parse(root, asset->String());
        if (!err.empty()) {
            Errorf("Failed to parse template (%s): %s", sourceName, err);
            return;
        }

        auto rootObj = root.get<picojson::object>();

        // Add defined components to the template root entity
        picojson::object componentList;
        auto componentsIter = rootObj.find("components");
        if (componentsIter != rootObj.end()) componentList = componentsIter->second.get<picojson::object>();

        for (auto comp : componentList) {
            if (comp.first.empty() || comp.first[0] == '_') continue;
            Assertf(comp.first != "name", "Template components can't override entity name: %s", comp.second.to_str());

            auto componentType = ecs::LookupComponent(comp.first);
            if (componentType != nullptr) {
                if (!componentType->LoadEntity(lock, ent, comp.second)) {
                    Errorf("Failed to load component, ignoring: %s", comp.first);
                }
            } else {
                Errorf("Unknown component, ignoring: %s", comp.first);
            }
        }

        // Add defined entities as sub-entities of the template root
        picojson::array entityList;
        auto entitiesIter = rootObj.find("entities");
        if (entitiesIter != rootObj.end()) entityList = entitiesIter->second.get<picojson::array>();

        // Find all named entities first so they can be referenced.
        for (auto value : entityList) {
            auto obj = value.get<picojson::object>();

            if (obj.count("name")) {
                auto relativeName = obj["name"].get<string>();
                ecs::Name name(relativeName, scope);
                if (!name) {
                    Errorf("Template %s contains invalid entity name: %s", sourceName, relativeName);
                    continue;
                }
                scene->NewPrefabEntity(lock, ent, name);
            }
        }

        std::vector<ecs::Entity> entities;
        for (auto value : entityList) {
            auto obj = value.get<picojson::object>();

            ecs::Entity newEntity;
            if (obj.count("name")) {
                auto relativeName = obj["name"].get<string>();
                ecs::Name entityName(relativeName, scope);
                if (!entityName) {
                    Errorf("Template %s contains invalid entity name: %s", sourceName, relativeName);
                    continue;
                }
                newEntity = scene->GetStagingEntity(entityName);
                if (!newEntity) {
                    Errorf("Skipping entity with invalid name: %s", entityName.String());
                    continue;
                }
            } else {
                newEntity = scene->NewPrefabEntity(lock, ent);
            }

            for (auto comp : obj) {
                if (comp.first.empty() || comp.first[0] == '_' || comp.first == "name") continue;

                auto componentType = ecs::LookupComponent(comp.first);
                if (componentType != nullptr) {
                    if (!componentType->LoadEntity(lock, newEntity, comp.second)) {
                        Errorf("Failed to load component, ignoring: %s", comp.first);
                    }
                } else {
                    Errorf("Unknown component, ignoring: %s", comp.first);
                }
            }

            if (newEntity.Has<ecs::TransformTree>(lock)) {
                auto &transform = newEntity.Get<ecs::TransformTree>(lock);
                if (!transform.parent && ent.Has<TransformTree>(lock)) transform.parent = ent;
                newEntity.Set<ecs::TransformSnapshot>(lock);
            }

            entities.emplace_back(newEntity);
        }

        for (auto &e : entities) {
            if (e.Has<ecs::Script>(lock)) { e.Get<ecs::Script>(lock).Prefab(lock, e); }
        }
    });
} // namespace ecs
