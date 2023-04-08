#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/GameEntities.hh"
#include "game/Scene.hh"

namespace sp::scripts {
    using namespace ecs;

    struct SoundOcclusion {
        PhysicsQuery::Handle<PhysicsQuery::Raycast> raycastQuery;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<Sounds, PhysicsQuery, TransformSnapshot>(lock)) return;

            auto &constSounds = ent.Get<const Sounds>(lock);
            if (constSounds.occlusionWeight <= 0.0f) return;

            auto listener = sp::entities::Head.Get(lock);
            if (listener.Has<TransformSnapshot>(lock)) {
                auto listenerPos = listener.Get<TransformSnapshot>(lock).GetPosition();
                auto soundPos = ent.Get<TransformSnapshot>(lock).GetPosition();
                auto rayToListener = listenerPos - soundPos;

                PhysicsQuery::Raycast nextQuery(glm::length(rayToListener),
                    PhysicsGroupMask(PHYSICS_GROUP_WORLD | PHYSICS_GROUP_INTERACTIVE | PHYSICS_GROUP_HELD_OBJECT |
                                     PHYSICS_GROUP_PLAYER_LEFT_HAND | PHYSICS_GROUP_PLAYER_RIGHT_HAND));
                nextQuery.direction = glm::normalize(rayToListener);
                nextQuery.relativeDirection = false;
                nextQuery.maxHits = 16;

                auto &physicsQuery = ent.Get<PhysicsQuery>(lock);

                if (!raycastQuery) {
                    raycastQuery = physicsQuery.NewQuery(nextQuery);
                } else {
                    auto &query = physicsQuery.Lookup(raycastQuery);
                    if (query.result) {
                        auto &sounds = ent.Get<Sounds>(lock);
                        sounds.occlusion = query.result->hits;
                        query = nextQuery;
                    }
                }
            }
        }
    };
    StructMetadata MetadataSoundOcclusion(typeid(SoundOcclusion));
    InternalScript<SoundOcclusion> soundOcclusion("sound_occlusion", MetadataSoundOcclusion);
} // namespace sp::scripts
