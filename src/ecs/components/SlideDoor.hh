#pragma once

#include <ecs/Components.hh>
#include <ecs/NamedEntity.hh>
#include <glm/glm.hpp>

namespace ecs {
    class SlideDoor {
    public:
        enum State { CLOSED, OPENED, OPENING, CLOSING };

        SlideDoor::State GetState(EntityManager &em);
        void Close(EntityManager &em);
        void Open(EntityManager &em);
        void ValidateDoor(EntityManager &em);
        void SetAnimation(NamedEntity &panel, glm::vec3 openDir);
        glm::vec3 LeftDirection(NamedEntity &panel);

        NamedEntity left;
        NamedEntity right;

        float width = 1.0f;
        float openTime = 0.5f;
        glm::vec3 forward = glm::vec3(0, 0, -1);
    };

    static Component<SlideDoor> ComponentSlideDoor("slideDoor"); // TODO: Rename this

    template<>
    bool Component<SlideDoor>::Load(SlideDoor &dst, const picojson::value &src);
} // namespace ecs
