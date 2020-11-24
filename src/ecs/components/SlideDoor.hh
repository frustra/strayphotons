#pragma once

#include <ecs/Components.hh>
#include <ecs/Ecs.hh>
#include <glm/glm.hpp>

namespace ecs {
    class SlideDoor {
    public:
        enum State { CLOSED, OPENED, OPENING, CLOSING };

        SlideDoor::State GetState(EntityManager &em);
        void Close(EntityManager &em);
        void Open(EntityManager &em);
        void ValidateDoor(EntityManager &em);
        void SetAnimation(Entity panel, glm::vec3 openDir);
        glm::vec3 LeftDirection(Entity panel);

        Tecs::Entity left;
        Tecs::Entity right;

        float width = 1.0f;
        float openTime = 0.5f;
        glm::vec3 forward = glm::vec3(0, 0, -1);
    };

    static Component<SlideDoor> ComponentSlideDoor("slideDoor"); // TODO: Rename this

    template<>
    bool Component<SlideDoor>::LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src);
} // namespace ecs
