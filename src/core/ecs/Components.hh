#pragma once

#include "ecs/ComponentMetadata.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/Name.hh"
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
    struct EntityScope {
        std::weak_ptr<sp::Scene> scene;
        Name prefix;
    };

    class ComponentBase {
    public:
        ComponentBase(const char *name) : name(name) {}

        virtual bool LoadEntity(Lock<AddRemove> lock,
            const EntityScope &scope,
            Entity &dst,
            const picojson::value &src) = 0;
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

        template<typename... Fields>
        Component(const char *name, Fields &&...fields) : Component(name) {
            (this->fields.emplace_back(fields), ...);
        }

        bool LoadFields(const EntityScope &scope, CompType &dst, const picojson::value &src) {
            for (auto &field : fields) {
                if (!field.Load(scope, &dst, src)) {
                    Errorf("Component %s has invalid field: %s", name, field.name);
                    return false;
                }
            }
            return true;
        }

        bool LoadEntity(Lock<AddRemove> lock,
            const EntityScope &scope,
            Entity &dst,
            const picojson::value &src) override {
            if (dst.Has<CompType>(lock)) {
                CompType srcComp;
                if (!LoadFields(scope, srcComp, src)) return false;
                if (!Load(scope, srcComp, src)) return false;
                Apply(srcComp, lock, dst);
                return true;
            } else {
                auto &comp = dst.Set<CompType>(lock);
                if (!LoadFields(scope, comp, src)) return false;
                return Load(scope, comp, src);
            }
        }

        bool SaveEntity(Lock<ReadAll> lock, picojson::value &dst, const Entity &src) override {
            static const CompType defaultValue = {};
            auto &comp = src.Get<CompType>(lock);

            for (auto &field : fields) {
                field.SaveIfChanged(dst, &comp, &defaultValue);
            }
            return Save(lock, dst, comp);
        }

        void ApplyComponent(Lock<ReadAll> srcLock, Entity src, Lock<AddRemove> dstLock, Entity dst) override {
            if (src.Has<CompType>(srcLock)) {
                static const CompType defaultValues = {};
                auto &srcComp = src.Get<CompType>(srcLock);

                if (!dst.Has<CompType>(dstLock)) {
                    dst.Set<CompType>(dstLock, srcComp);
                } else if constexpr (std::is_same<CompType, Name>::value) {
                    auto &dstName = dst.Get<Name>(dstLock);
                    Assertf(dstName == srcComp, "ApplyComponent called with mismatched ecs::Name!");
                } else if constexpr (std::is_same<CompType, SceneInfo>::value) {
                    auto &dstInfo = dst.Get<SceneInfo>(dstLock);
                    Assertf(dstInfo.liveId == srcComp.liveId,
                        "ApplyComponent called with mismatched SceneInfo::liveId!");
                    Assertf(dstInfo.stagingId == srcComp.stagingId,
                        "ApplyComponent called with mismatched SceneInfo::stagingId!");
                    Assertf(dstInfo.nextStagingId == srcComp.nextStagingId,
                        "ApplyComponent called with mismatched SceneInfo::nextStagingId!");
                    Assertf(dstInfo.prefabStagingId == srcComp.prefabStagingId,
                        "ApplyComponent called with mismatched SceneInfo::prefabStagingId!");
                    Assertf(dstInfo.priority == srcComp.priority,
                        "ApplyComponent called with mismatched SceneInfo::priority!");
                } else {
                    // Merge existing component with a new one
                    auto &dstComp = dst.Get<CompType>(dstLock);
                    if (fields.empty()) Warnf("Component has no fields defined: %s", typeid(CompType).name());

                    for (auto &field : fields) {
                        field.Apply(&dstComp, &srcComp, &defaultValues);
                    }

                    Apply(srcComp, dstLock, dst);
                }
            }
        }

        static bool Load(const EntityScope &scope, CompType &dst, const picojson::value &src) {
            // Custom field serialization is always called, default to no-op.
            return true;
        }

        static bool Save(Lock<Read<Name>> lock, picojson::value &dst, const CompType &src) {
            // Custom field serialization is always called, default to no-op.
            return true;
        }

        static void Apply(const CompType &src, Lock<AddRemove> lock, Entity dst) {}

        bool operator==(const Component<CompType> &other) const {
            return strcmp(name, other.name) == 0;
        }

        bool operator!=(const Component<CompType> &other) const {
            return !(*this == other);
        }

    private:
        std::vector<ComponentField> fields;
    };
}; // namespace ecs
