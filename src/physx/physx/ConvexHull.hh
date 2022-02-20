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
    struct ConvexHull {
        std::vector<glm::vec3> points;
        std::vector<glm::ivec3> triangles;

        std::shared_ptr<physx::PxConvexMesh> pxMesh;
    };

    struct ConvexHullSet {
        vector<ConvexHull> hulls;
        robin_hood::unordered_flat_set<int> bufferIndexes;
        bool decomposed;
    };

    namespace ConvexHullBuilding {
        // Builds convex hull set for a model without caching
        void BuildConvexHulls(ConvexHullSet *set, const Gltf &model, const gltf::Mesh &mesh, bool decompHull);
    } // namespace ConvexHullBuilding
} // namespace sp
