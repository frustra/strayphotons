#pragma once

#include "physx/PhysxManager.hh"

#include <ecs/Ecs.hh>

namespace sp {
    class InputManager;
    class GameLogic;
} // namespace sp

namespace ecs {
    class LightGunSystem {
    public:
        LightGunSystem(EntityManager *entities,
                       sp::InputManager *input,
                       sp::PhysxManager *physics,
                       sp::GameLogic *logic);

        ~LightGunSystem();

        bool Frame(float dtSinceLastFrame);
        void SuckLight(Entity &gun);
        void ShootLight(Entity &gun);

    private:
        Entity EntityRaycast(Entity &origin);

        EntityManager *entities;
        sp::InputManager *input;
        sp::PhysxManager *physics;
        sp::GameLogic *logic;
    };
} // namespace ecs
