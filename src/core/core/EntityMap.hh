#pragma once

#include "core/Common.hh"

#include <Tecs.hh>
#include <map>
#include <vector>

namespace sp {
    // TODO: Implement using a std::vector that uses Tecs::Entity index and generation
    template<typename T>
    class EntityMap : public std::map<Tecs::Entity, T> {};
} // namespace sp
