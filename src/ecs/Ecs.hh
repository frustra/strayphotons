#pragma once

// This is the only file needed to be included to use the ECS.
// Impl is needed to be included since many functions are templated
// Defn/Impl is split between headers because of cyclic dependencies

#include "ecs/Entity.hh"
#include "ecs/EntityManager.hh"

#include "ecs/EntityImpl.hh"
#include "ecs/EntityManagerImpl.hh"
