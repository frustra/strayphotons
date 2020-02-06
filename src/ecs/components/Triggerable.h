#pragma once

#include <ecs/Components.hh>

namespace ecs
{
    struct Triggerable
    {
        // TODO: information that help determine which trigger areas 
        // will trigger for this entity
    };

    static Component<Triggerable> ComponentTriggerable("triggerable");
};