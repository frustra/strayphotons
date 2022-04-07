#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/SceneInfo.hh"
#include "game/Scene.hh"

#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace picojson {
    class value;
}

namespace sp {
    class Scene;
} // namespace sp

namespace ecs {
    struct Name;
    using ScenePtr = std::weak_ptr<sp::Scene>;

    class ComponentBase {
    public:
        ComponentBase(const char *name) : name(name) {}

        virtual bool LoadEntity(Lock<AddRemove> lock, Entity &dst, const picojson::value &src) = 0;
        virtual bool SaveEntity(Lock<ReadAll> lock, picojson::value &dst, const Entity &src) = 0;
        virtual void ApplyComponent(Lock<ReadAll> src, Entity srcEnt, Lock<AddRemove> dst, Entity dstEnt) = 0;

        const char *name;
    };

    void RegisterComponent(const char *name, ComponentBase *comp);
    ComponentBase *LookupComponent(const std::string name);

    template<typename CompType>
    class Component : public ComponentBase {
    public:
        Component() : ComponentBase("") {}
        Component(const char *name) : ComponentBase(name) {
            auto existing = dynamic_cast<Component<CompType> *>(LookupComponent(std::string(name)));
            if (existing == nullptr) {
                RegisterComponent(name, this);
            } else if (*this != *existing) {
                throw std::runtime_error("Duplicate component type registered: " + std::string(name));
            }
        }

        bool LoadEntity(Lock<AddRemove> lock, Entity &dst, const picojson::value &src) override {
            ScenePtr scene;
            Name scope;
            if (dst.Has<SceneInfo>(lock)) {
                auto &sceneInfo = dst.Get<SceneInfo>(lock);
                scene = sceneInfo.scene;
                if (sceneInfo.prefabStagingId.Has<Name>(lock)) {
                    scope = sceneInfo.prefabStagingId.Get<Name>(lock);
                } else {
                    auto parentScene = scene.lock();
                    if (parentScene) scope.scene = parentScene->name;
                    if (sceneInfo.prefabStagingId) {
                        scope.entity = std::to_string(sceneInfo.prefabStagingId.generation);
                    }
                }
            }
            auto &comp = dst.Set<CompType>(lock);
            return Load(scene, scope, comp, src);
        }

        bool SaveEntity(Lock<ReadAll> lock, picojson::value &dst, const Entity &src) override {
            auto &comp = src.Get<CompType>(lock);
            return Save(lock, dst, comp);
        }

        void ApplyComponent(Lock<ReadAll> srcLock, Entity src, Lock<AddRemove> dstLock, Entity dst) override {
            if (src.Has<CompType>(srcLock)) {
                auto &comp = src.Get<CompType>(srcLock);
                Apply(comp, dstLock, dst);
            }
        }

        static bool Load(ScenePtr scenePtr, const Name &scope, CompType &dst, const picojson::value &src) {
            std::cerr << "Calling undefined Load on Compoent type: " << typeid(CompType).name() << std::endl;
            return false;
        }

        static bool Save(Lock<Read<Name>> lock, picojson::value &dst, const CompType &src) {
            std::cerr << "Calling undefined Save on Compoent type: " << typeid(CompType).name() << std::endl;
            return false;
        }

        static void Apply(const CompType &src, Lock<AddRemove> lock, Entity dst) {
            if (!dst.Has<CompType>(lock)) dst.Set<CompType>(lock, src);
        }

        bool operator==(const Component<CompType> &other) const {
            return strcmp(name, other.name) == 0;
        }

        bool operator!=(const Component<CompType> &other) const {
            return !(*this == other);
        }
    };
}; // namespace ecs
