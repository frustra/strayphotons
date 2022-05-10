#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    std::array audioScripts = {
        InternalScript("sound_occlusion",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (!ent.Has<Sounds, PhysicsQuery, TransformSnapshot>(lock)) return;

                auto &constSounds = ent.Get<const Sounds>(lock);
                if (constSounds.occlusionWeight <= 0.0f) return;

                struct Data {
                    EntityRef listener;
                    EntityRef listenerFallback;
                    PhysicsQuery::Handle<PhysicsQuery::Raycast> raycastQuery;
                } data;

                if (state.userData.has_value()) {
                    data = std::any_cast<Data>(state.userData);
                } else {
                    data.listener = EntityRef(Name{"vr", "hmd"});
                    data.listenerFallback = EntityRef(Name{"player", "flatview"});
                }

                auto listener = data.listener.Get(lock);
                if (!listener) listener = data.listenerFallback.Get(lock);

                if (listener.Has<TransformSnapshot>(lock)) {
                    auto listenerPos = listener.Get<TransformSnapshot>(lock).GetPosition();
                    auto soundPos = ent.Get<TransformSnapshot>(lock).GetPosition();
                    auto rayToListener = listenerPos - soundPos;

                    PhysicsQuery::Raycast nextQuery(glm::length(rayToListener));
                    nextQuery.direction = glm::normalize(rayToListener);
                    nextQuery.relativeDirection = false;
                    nextQuery.maxHits = 16;

                    auto &physicsQuery = ent.Get<PhysicsQuery>(lock);

                    if (!data.raycastQuery) {
                        data.raycastQuery = physicsQuery.NewQuery(nextQuery);
                    } else {
                        auto &query = physicsQuery.Lookup(data.raycastQuery);
                        if (query.result) {
                            auto &sounds = ent.Get<Sounds>(lock);
                            sounds.occlusion = query.result->hits;
                            query = nextQuery;
                        }
                    }
                }

                state.userData = data;
            }),
    };
} // namespace ecs
