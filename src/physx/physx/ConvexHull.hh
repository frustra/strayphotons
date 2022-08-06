#pragma once

#include "core/Common.hh"

#include <atomic>
#include <glm/glm.hpp>
#include <memory>
#include <robin_hood.h>
#include <vector>

namespace physx {
    class PxCollection;
    class PxConvexMesh;
    class PxCooking;
    class PxPhysics;
    class PxSerializationRegistry;
} // namespace physx

namespace sp::gltf {
    struct Mesh;
}

namespace sp {
    class Asset;
    struct HullSettings;

    typedef std::shared_ptr<physx::PxConvexMesh> ConvexHull;

    struct ConvexHullSet {
        // Collection must be destroyed after hulls
        std::vector<uint8_t> collectionBuffer;
        std::shared_ptr<physx::PxCollection> collection;

        std::vector<ConvexHull> hulls;

        AsyncPtr<Gltf> sourceModel;
        AsyncPtr<HullSettings> sourceSettings;
    };

    namespace hullgen {
        // Builds convex hull set for a model without caching
        std::shared_ptr<ConvexHullSet> BuildConvexHulls(physx::PxCooking &cooking,
            physx::PxPhysics &physics,
            const AsyncPtr<Gltf> &model,
            const AsyncPtr<HullSettings> &settings);

        std::shared_ptr<ConvexHullSet> LoadCollisionCache(physx::PxSerializationRegistry &registry,
            const AsyncPtr<Gltf> &model,
            const AsyncPtr<HullSettings> &settings);
        void SaveCollisionCache(physx::PxSerializationRegistry &registry,
            const AsyncPtr<Gltf> &model,
            const AsyncPtr<HullSettings> &settings,
            const ConvexHullSet &set);
    } // namespace hullgen
} // namespace sp
