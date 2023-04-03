#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/GameEntities.hh"
#include "game/Scene.hh"

namespace sp::scripts {
    using namespace ecs;

    struct SoundOcclusion {
        PhysicsQuery::Handle<PhysicsQuery::Raycast> raycastQuery;

        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            if (!entLock.Has<Sounds, PhysicsQuery, TransformSnapshot>()) return;

            auto &constSounds = entLock.Get<const Sounds>();
            if (constSounds.occlusionWeight <= 0.0f) return;

            auto listener = sp::entities::Head.Get(entLock);
            if (listener.Has<TransformSnapshot>(entLock)) {
                auto listenerPos = listener.Get<TransformSnapshot>(entLock).GetPosition();
                auto soundPos = entLock.Get<TransformSnapshot>().GetPosition();
                auto rayToListener = listenerPos - soundPos;

                PhysicsQuery::Raycast nextQuery(glm::length(rayToListener),
                    PhysicsGroupMask(PHYSICS_GROUP_WORLD | PHYSICS_GROUP_INTERACTIVE | PHYSICS_GROUP_HELD_OBJECT |
                                     PHYSICS_GROUP_PLAYER_LEFT_HAND | PHYSICS_GROUP_PLAYER_RIGHT_HAND));
                nextQuery.direction = glm::normalize(rayToListener);
                nextQuery.relativeDirection = false;
                nextQuery.maxHits = 16;

                auto &physicsQuery = entLock.Get<PhysicsQuery>();

                if (!raycastQuery) {
                    raycastQuery = physicsQuery.NewQuery(nextQuery);
                } else {
                    auto &query = physicsQuery.Lookup(raycastQuery);
                    if (query.result) {
                        auto &sounds = entLock.Get<Sounds>();
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
