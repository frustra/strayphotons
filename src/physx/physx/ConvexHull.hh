#pragma once

#include "core/Common.hh"

#include <atomic>
#include <memory>
#include <robin_hood.h>

namespace physx {
    class PxConvexMesh;
}

namespace sp {
    class Model;

    struct ConvexHull {
        uint32 pointCount;
        uint32 pointByteStride;
        uint32 triangleCount;
        uint32 triangleByteStride;

        float *points;
        int *triangles;

        std::shared_ptr<physx::PxConvexMesh> pxMesh;
    };

    struct ConvexHullSet {
        vector<ConvexHull> hulls;
        robin_hood::unordered_flat_set<int> bufferIndexes;
        bool decomposed;

        std::atomic_flag valid;
        
        bool Valid() const {
            return valid.test();
        }

        void WaitUntilValid() const {
            while (!valid.test()) {
                valid.wait(false);
            }
        }
    };

    namespace ConvexHullBuilding {
        // Builds convex hull set for a model without caching
        void BuildConvexHulls(ConvexHullSet *set, const Model &model, bool decompHull);
    } // namespace ConvexHullBuilding
} // namespace sp
