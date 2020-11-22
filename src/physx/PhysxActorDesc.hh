#pragma once

#include <PxPhysicsAPI.h>

namespace sp {
    struct PhysxActorDesc {
        bool dynamic = true;
        bool kinematic = false; // only dynamic actors can be kinematic
        bool decomposeHull = false;

        // Initial values
        physx::PxTransform transform = physx::PxTransform(physx::PxVec3(0));
        physx::PxMeshScale scale = physx::PxMeshScale();
        float density = 1.0f;
    };
} // namespace sp