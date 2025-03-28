/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptManager.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"

#include <picojson/picojson.h>

namespace ecs {
    struct TemplateParser {
        std::shared_ptr<sp::Scene> scene;
        Entity rootEnt;
        size_t prefabScriptId;
        std::string sourceName;

        sp::AsyncPtr<sp::Asset> assetPtr;

        bool hasRootOverride = false; // True if "components" field is defined
        FlatEntity rootComponents; // Represented by the "components" template field
        std::vector<std::pair<std::string, FlatEntity>> entityList; // Represented by the "entities" template field

        TemplateParser(const std::shared_ptr<sp::Scene> &scene,
            const Entity &rootEnt,
            size_t prefabScriptId,
            std::string source)
            : scene(scene), rootEnt(rootEnt), prefabScriptId(prefabScriptId), sourceName(source) {
            if (sourceName.empty()) return;
            assetPtr = sp::Assets().Load("scenes/templates/" + sourceName + ".json", sp::AssetType::Bundled, true);
        }

        // The provided scope is used for debug logging only, the real scope of the resulting entities
        // is provided to the ApplyComponents() and AddEntities() functions
        bool Parse(const EntityScope &parseScope) {
            if (!assetPtr) return false;
            ZoneScoped;

            Debugf("Parsing template: %s in scope '%s'", sourceName, parseScope.String());

            auto asset = assetPtr->Get();
            if (!asset) {
                Errorf("Template not found: %s", sourceName);
                return false;
            }

            picojson::value rootValue;
            string err = picojson::parse(rootValue, asset->String());
            if (!err.empty()) {
                Errorf("Failed to parse template (%s): %s", sourceName, err);
                return false;
            }

            if (!rootValue.is<picojson::object>()) {
                Errorf("Template parameters must be an object (%s)", sourceName);
                return false;
            }

            auto &root = rootValue.get<picojson::object>();

            auto entitiesIter = root.find("entities");
            if (entitiesIter != root.end()) {
                if (!entitiesIter->second.is<picojson::array>()) {
                    Errorf("Template 'entities' field must be array (%s): %s",
                        sourceName,
                        entitiesIter->second.to_str());
                    return false;
                }

                for (auto &entityObj : entitiesIter->second.get<picojson::array>()) {
                    if (!entityObj.is<picojson::object>()) {
                        Errorf("Template 'entities' entry must be object (%s), ignoring: %s",
                            sourceName,
                            entityObj.to_str());
                        continue;
                    }

                    auto &entSrc = entityObj.get<picojson::object>();
                    const std::string *relativeName = nullptr;
                    if (entSrc.count("name") && entSrc["name"].is<string>()) {
                        relativeName = &entSrc["name"].get<string>();
                        if (*relativeName == "scoperoot") {
                            Errorf("Entity name 'scoperoot' in template not allowed (%s), ignoring", sourceName);
                            continue;
                        }
                    }
                    auto &entDst = entityList.emplace_back();
                    if (relativeName) entDst.first = *relativeName;

                    for (auto &comp : entSrc) {
                        if (comp.first.empty() || comp.first[0] == '_' || comp.first == "name") continue;

                        auto componentType = LookupComponent(comp.first);
                        if (componentType != nullptr) {
                            if (!componentType->LoadEntity(entDst.second, comp.second)) {
                                Errorf("Failed to load component in template (%s), ignoring: %s",
                                    sourceName,
                                    comp.first);
                            }
                        } else {
                            Errorf("Unknown component in template (%s), ignoring: %s", sourceName, comp.first);
                        }
                    }
                }
            }

            auto componentsIter = root.find("components");
            if (componentsIter != root.end()) {
                auto &componentsObj = componentsIter->second;
                if (!componentsObj.is<picojson::object>()) {
                    Errorf("Template 'components' field must be object (%s): %s", sourceName, componentsObj.to_str());
                    return false;
                }

                hasRootOverride = true;

                auto &entSrc = componentsObj.get<picojson::object>();
                if (entSrc.count("name")) {
                    Errorf("Template 'components' field cannot override 'name' (%s), ignoring: %s",
                        sourceName,
                        entSrc["name"].to_str());
                }

                for (auto &comp : entSrc) {
                    if (comp.first.empty() || comp.first[0] == '_' || comp.first == "name") continue;

                    auto componentType = LookupComponent(comp.first);
                    if (componentType != nullptr) {
                        if (!componentType->LoadEntity(rootComponents, comp.second)) {
                            Errorf("Failed to load component in template (%s), ignoring: %s", sourceName, comp.first);
                        }
                    } else {
                        Errorf("Unknown component in template (%s), ignoring: %s", sourceName, comp.first);
                    }
                }
            }
            return true;
        }

        // Add defined components to the template root entity with the given name
        Entity ApplyComponents(const Lock<AddRemove> &lock, EntityScope scope, Transform offset = {}) {
            ZoneScoped;

            Entity newEntity = scene->NewPrefabEntity(lock, rootEnt, prefabScriptId, "scoperoot", scope);
            ForEachComponent([&](const std::string &name, const ComponentBase &comp) {
                comp.SetComponent(lock, scope, newEntity, rootComponents);
            });
            EntityRef newRef(newEntity);

            if (newEntity.Has<TransformTree>(lock)) {
                auto &transform = newEntity.Get<TransformTree>(lock);
                if (!transform.parent) {
                    if (rootEnt.Has<TransformTree>(lock) && rootEnt != newRef) {
                        transform.parent = rootEnt;
                    }
                    transform.pose = offset * transform.pose.Get();
                }
            }
            return newEntity;
        }

        // Add defined entities as sub-entities of the template root
        void AddEntities(const Lock<AddRemove> &lock, EntityScope scope, Transform offset = {}) {
            ZoneScoped;

            std::vector<Entity> scriptEntities;
            for (auto &[relativeName, flatEnt] : entityList) {
                Entity newEntity = scene->NewPrefabEntity(lock, rootEnt, prefabScriptId, relativeName, scope);
                if (!newEntity) {
                    // Most llkely a duplicate entity or invalid name
                    Errorf("Failed to create template entity (%s), ignoring: '%s'", sourceName, relativeName);
                    continue;
                }
                EntityRef newRef(newEntity);

                ForEachComponent([&](const std::string &name, const ComponentBase &comp) {
                    comp.SetComponent(lock, scope, newEntity, flatEnt);
                });

                if (newEntity.Has<TransformTree>(lock)) {
                    auto &transform = newEntity.Get<TransformTree>(lock);
                    if (!transform.parent) {
                        if (rootEnt.Has<TransformTree>(lock) && rootEnt != newRef) {
                            transform.parent = rootEnt;
                        }
                        transform.pose = offset * transform.pose.Get();
                    }
                }
                if (newEntity.Has<Scripts>(lock)) {
                    scriptEntities.push_back(newEntity);
                }
            }

            auto &scriptManager = GetScriptManager();
            for (auto &e : scriptEntities) {
                scriptManager.RunPrefabs(lock, e);
            }
        }
    };

    struct TemplatePrefab {
        std::string source;

        void Prefab(const ScriptState &state,
            const std::shared_ptr<sp::Scene> &scene,
            Lock<AddRemove> lock,
            Entity ent) {
            ZoneScoped;
            ZoneStr(source);

            EntityScope scope = Name(scene->data->name, "");
            if (ent.Has<Name>(lock)) scope = ent.Get<Name>(lock);

            TemplateParser parser(scene, ent, state.GetInstanceId(), source);
            if (!parser.Parse(scope)) return;

            Entity rootOverride = parser.ApplyComponents(lock, scope);
            parser.AddEntities(lock, scope);
            if (rootOverride.Has<Scripts>(lock)) {
                GetScriptManager().RunPrefabs(lock, rootOverride);
            }
        }
    };
    StructMetadata MetadataTemplatePrefab(typeid(TemplatePrefab),
        "TemplatePrefab",
        "",
        StructField::New("source", &TemplatePrefab::source));
    PrefabScript<TemplatePrefab> templatePrefab("template", MetadataTemplatePrefab);

    struct TilePrefab {
        glm::ivec2 count = {1, 1};
        glm::vec2 stride = {1, 1};
        std::string axes = "xy";
        std::string surfaceTemplate, edgeTemplate, cornerTemplate;

        void Prefab(const ScriptState &state,
            const std::shared_ptr<sp::Scene> &scene,
            Lock<AddRemove> lock,
            Entity ent) {
            ZoneScoped;

            if (axes.size() != 2) {
                Errorf("'%s' axes are invalid, must tile on 2 unique axes: %s", axes, ToString(lock, ent));
                return;
            }
            std::pair<int, int> axesIndex = {axes[0] - 'x', axes[1] - 'x'};
            if (axesIndex.first < 0 || axesIndex.second < 0 || axesIndex.first > 2 || axesIndex.second > 2) {
                Errorf("'%s' axes are invalid, must tile on x, y, or z: %s", axes, ToString(lock, ent));
                return;
            } else if (axesIndex.first == axesIndex.second) {
                Errorf("'%s' axes are invalid, must tile on 2 unique axes: %s", axes, ToString(lock, ent));
                return;
            }

            EntityScope rootScope = Name(scene->data->name, "");
            if (ent.Has<Name>(lock)) rootScope = ent.Get<Name>(lock);

            TemplateParser surface(scene, ent, state.GetInstanceId(), surfaceTemplate);
            if (!surface.Parse(rootScope)) return;

            TemplateParser edge(scene, ent, state.GetInstanceId(), edgeTemplate);
            auto haveEdge = edge.Parse(rootScope);

            TemplateParser corner(scene, ent, state.GetInstanceId(), cornerTemplate);
            auto haveCorner = corner.Parse(rootScope);

            glm::vec3 normal{1, 1, 1};
            normal[axesIndex.first] = 0;
            normal[axesIndex.second] = 0;

            for (int x = 0; x < count.x; x++) {
                for (int y = 0; y < count.y; y++) {
                    auto offset2D = (glm::vec2{float(x), float(y)} + glm::vec2(0.5)) * stride;

                    glm::vec3 offset3D = glm::vec3(0);
                    offset3D[axesIndex.first] = offset2D.x;
                    offset3D[axesIndex.second] = offset2D.y;

                    EntityScope tileScope = Name(std::to_string(x) + "_" + std::to_string(y), rootScope);

                    Entity tileEnt = surface.ApplyComponents(lock, tileScope, offset3D);
                    if (!tileEnt) {
                        // Most llkely a duplicate entity or invalid name
                        Errorf("Failed to create tiled template entity (%s), ignoring: '%s'",
                            surface.sourceName,
                            tileScope.String());
                        continue;
                    }

                    auto &tileSignals = tileEnt.Get<SignalOutput>(lock).signals;
                    tileSignals.emplace("tile.x", x);
                    tileSignals.emplace("tile.y", y);
                    surface.AddEntities(lock, tileScope, offset3D);

                    auto xEdge = x == 0 || x == (count.x - 1);
                    auto yEdge = y == 0 || y == (count.y - 1);

                    if (xEdge && yEdge) {
                        if (haveCorner) {
                            Transform transform;
                            if (x != 0 && y != 0) {
                                transform.Rotate(M_PI, normal);
                            } else if (x != 0) {
                                transform.Rotate(-M_PI / 2, normal);
                            } else if (y != 0) {
                                transform.Rotate(M_PI / 2, normal);
                            }
                            transform.Translate(offset3D);
                            corner.ApplyComponents(lock, tileScope, transform);
                            corner.AddEntities(lock, tileScope, transform);
                        }
                    } else if (xEdge || yEdge) {
                        if (haveEdge) {
                            Transform transform;
                            if (x == (count.x - 1)) {
                                transform.Rotate(-M_PI / 2, normal);
                            } else if (y == (count.y - 1)) {
                                transform.Rotate(M_PI, normal);
                            } else if (x == 0) {
                                transform.Rotate(M_PI / 2, normal);
                            }
                            transform.Translate(offset3D);
                            edge.ApplyComponents(lock, tileScope, transform);
                            edge.AddEntities(lock, tileScope, transform);
                        }
                    }
                    if (tileEnt.Has<Scripts>(lock)) {
                        GetScriptManager().RunPrefabs(lock, tileEnt);
                    }
                }
            }
        }
    };
    StructMetadata MetadataTilePrefab(typeid(TilePrefab),
        "TilePrefab",
        "",
        StructField::New("count", &TilePrefab::count),
        StructField::New("stride", &TilePrefab::stride),
        StructField::New("axes", &TilePrefab::axes),
        StructField::New("surface", &TilePrefab::surfaceTemplate),
        StructField::New("edge", &TilePrefab::edgeTemplate),
        StructField::New("corner", &TilePrefab::cornerTemplate));
    PrefabScript<TilePrefab> tilePrefab("tile", MetadataTilePrefab);
} // namespace ecs
