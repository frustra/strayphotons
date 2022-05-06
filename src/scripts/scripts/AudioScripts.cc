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
                    std::optional<PhysicsQuery::Raycast> lastQuery;
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

                    if (!data.lastQuery) {
                        physicsQuery.queries.emplace_back(nextQuery);
                        data.lastQuery = nextQuery;
                    } else {
                        for (size_t i = 0; i < physicsQuery.queries.size(); i++) {
                            auto query = std::get_if<PhysicsQuery::Raycast>(&physicsQuery.queries[i]);
                            if (query && data.lastQuery == *query) {
                                if (query->result) {
                                    auto &sounds = ent.Get<Sounds>(lock);
                                    sounds.occlusion = query->result.value().hits;
                                    physicsQuery.queries[i] = nextQuery;
                                    data.lastQuery = nextQuery;
                                }
                                break;
                            }
                        }
                    }
                }

                state.userData = data;
            }),
    };
} // namespace ecs
