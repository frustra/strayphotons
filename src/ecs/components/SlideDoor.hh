#pragma once

#include <ecs/Components.hh>
#include <ecs/Ecs.hh>
#include <glm/glm.hpp>

namespace ecs {
    class Animation;

    class SlideDoor {
    public:
        enum State { CLOSED, OPENED, OPENING, CLOSING };

        SlideDoor::State GetState(Lock<Read<Animation>> lock) const;
        void Close(Lock<Write<Animation>> lock) const;
        void Open(Lock<Write<Animation>> lock) const;
        void ValidateDoor(Lock<Read<Animation>> lock) const;

        Tecs::Entity left;
        Tecs::Entity right;
    };

    static Component<SlideDoor> ComponentSlideDoor("slideDoor"); // TODO: Rename this

    template<>
    bool Component<SlideDoor>::Load(Lock<Read<ecs::Name>> lock, SlideDoor &dst, const picojson::value &src);
} // namespace ecs
