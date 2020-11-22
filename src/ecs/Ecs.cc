#include "Ecs.hh"

#include "EcsImpl.hh"

#include <typeindex>

namespace ecs {
    void Entity::Destroy() {
        em->Emit(e.id, EntityDestruction());
        {
            auto lock = em->tecs.StartTransaction<Tecs::AddRemove>();
            e.Destroy(lock);
        }
    }

    EntityManager::EntityManager() {}

    Entity EntityManager::NewEntity() {
        auto lock = tecs.StartTransaction<Tecs::AddRemove>();
        return Entity(this, lock.NewEntity());
    }

    void EntityManager::DestroyAll() {
        std::vector<Tecs::Entity> removeList;
        {
            auto lock = tecs.StartTransaction<>();
            removeList.assign(lock.Entities().begin(), lock.Entities().end());
        }
        for (auto e : removeList) {
            Emit(e.id, EntityDestruction());
        }
        {
            auto lock = tecs.StartTransaction<Tecs::AddRemove>();
            for (auto e : removeList) {
                e.Destroy(lock);
            }
        }
    }

    Subscription::Subscription() {}

    Subscription::Subscription(EntityManager *manager,
                               std::list<GenericEntityCallback> *list,
                               std::list<GenericEntityCallback>::iterator &c)
        : manager(manager), entityConnectionList(list), entityConnection(c) {}

    Subscription::Subscription(EntityManager *manager,
                               std::list<GenericCallback> *list,
                               std::list<GenericCallback>::iterator &c)
        : manager(manager), connectionList(list), connection(c) {}

    bool Subscription::IsActive() const {
        return entityConnectionList != nullptr || connectionList != nullptr;
    }

    void Subscription::Unsubscribe() {
        if (manager != nullptr) {
            std::lock_guard<std::recursive_mutex> lock(manager->signalLock);

            if (entityConnectionList != nullptr) {
                entityConnectionList->erase(entityConnection);
                entityConnectionList = nullptr;
            } else if (connectionList != nullptr) {
                connectionList->erase(connection);
                connectionList = nullptr;
            }
        }
    }
} // namespace ecs
