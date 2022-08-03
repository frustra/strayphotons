#pragma once

#include "core/Common.hh"

#include <atomic>
#include <glm/glm.hpp>
#include <memory>
#include <robin_hood.h>
#include <vector>

namespace physx {
    class PxConvexMesh;
}

namespace sp::gltf {
    struct Mesh;
}

namespace sp {
    class Asset;
    struct HullSettings;

    struct ConvexHull {
        std::vector<glm::vec3> points;
        std::vector<glm::ivec3> triangles;

        std::shared_ptr<physx::PxConvexMesh> pxMesh;
    };

    struct ConvexHullSet {
        std::vector<ConvexHull> hulls;
        std::shared_ptr<const Asset> source;
        std::shared_ptr<const Asset> config;
    };

    namespace hullgen {
        // Builds convex hull set for a model without caching
        std::shared_ptr<ConvexHullSet> BuildConvexHulls(const Gltf &model, const HullSettings &hullSettings);

        std::shared_ptr<ConvexHullSet> LoadCollisionCache(const Gltf &model, const HullSettings &hullSettings);
        void SaveCollisionCache(const Gltf &model, const HullSettings &hullSettings, const ConvexHullSet &set);
    } // namespace hullgen
} // namespace sp
