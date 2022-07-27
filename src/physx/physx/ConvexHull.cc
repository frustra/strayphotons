#include "ConvexHull.hh"

#include "assets/Gltf.hh"
#include "assets/GltfImpl.hh"
#include "core/Logging.hh"

#include <unordered_set>

#define ENABLE_VHACD_IMPLEMENTATION 1
#include <VHACD.h>

#define GLM_FORCE_CXX11
#include <glm/gtx/hash.hpp>

namespace sp {
    class VHACDCallback : public VHACD::IVHACD::IUserCallback {
    public:
        VHACDCallback() {}
        ~VHACDCallback() {}

        void Update(const double overallProgress,
            const double stageProgress,
            const char *const stage,
            const char *operation) override {
            Logf("VHACD %d (%s) %s", (int)(overallProgress + 0.5), stage, operation);
        }
    };

    static_assert(sizeof(VHACD::Triangle) == sizeof(glm::ivec3), "Unexpected vhacd triangle type");

    void decomposeConvexHullsForPrimitive(ConvexHullSet *set,
        const Gltf &model,
        const gltf::Mesh &mesh,
        const gltf::Mesh::Primitive &prim) {
        ZoneScoped;
        set->decomposed = true;

        Assert(prim.drawMode == gltf::Mesh::DrawMode::Triangles, "primitive draw mode must be triangles");
        std::vector<glm::vec3> points(prim.positionBuffer.Count());
        std::vector<uint32_t> indices(prim.indexBuffer.Count());

        for (size_t i = 0; i < prim.positionBuffer.Count(); i++) {
            points[i] = prim.positionBuffer.Read(i);
        }
        for (size_t i = 0; i < prim.indexBuffer.Count(); i++) {
            indices[i] = prim.indexBuffer.Read(i);
        }

        static VHACDCallback vhacdCallback;

        auto interfaceVHACD = VHACD::CreateVHACD();

        VHACD::IVHACD::Parameters params;
        params.m_callback = &vhacdCallback;
        // params.m_shrinkWrap = false;
        // params.m_oclAcceleration = false;
        // params.m_resolution = 1000000;
        // params.m_convexhullDownsampling = 8;

        bool res = interfaceVHACD->Compute(reinterpret_cast<const float *>(points.data()),
            points.size(),
            indices.data(),
            indices.size() / 3,
            params);
        Assert(res, "building convex decomposition");

        for (uint32 i = 0; i < interfaceVHACD->GetNConvexHulls(); i++) {
            VHACD::IVHACD::ConvexHull ihull;
            interfaceVHACD->GetConvexHull(i, ihull);
            if (ihull.m_points.size() < 3 || ihull.m_triangles.empty()) continue;

            ConvexHull hull;
            hull.points.reserve(ihull.m_points.size());
            for (auto &point : ihull.m_points) {
                hull.points.emplace_back(point.mX, point.mY, point.mZ);
            }

            hull.triangles.reserve(ihull.m_triangles.size());
            auto src = reinterpret_cast<glm::ivec3 *>(ihull.m_triangles.data());
            hull.triangles.assign(src, src + ihull.m_triangles.size());

            set->hulls.push_back(hull);
            Logf("Adding VHACD hull, %d points, %d triangles", hull.points.size(), hull.triangles.size());
        }

        interfaceVHACD->Clean();
        interfaceVHACD->Release();
    }

    void buildConvexHullForPrimitive(ConvexHullSet *set,
        const Gltf &model,
        const gltf::Mesh &mesh,
        const gltf::Mesh::Primitive &prim) {
        ZoneScoped;
        set->decomposed = false;

        std::unordered_set<uint32_t> visitedIndexes;
        std::vector<VHACD::Vertex> points;
        points.reserve(prim.positionBuffer.Count());

        for (size_t i = 0; i < prim.indexBuffer.Count(); i++) {
            uint32_t index = prim.indexBuffer.Read(i);
            if (visitedIndexes.count(index) || index >= prim.positionBuffer.Count()) continue;

            auto value = prim.positionBuffer.Read(index);
            points.emplace_back(value.x, value.y, value.z);
            visitedIndexes.insert(index);
        }

        VHACD::QuickHullImpl hullImpl;
        hullImpl.computeConvexHull(points, 255 /* PhysX hull size limit */);
        auto &vertices = hullImpl.getVertices();
        auto &indices = hullImpl.getIndices();
        if (vertices.size() < 3 || indices.empty()) return;

        auto &hull = set->hulls.emplace_back();
        hull.points.reserve(vertices.size());
        for (auto &point : vertices) {
            hull.points.emplace_back(point.mX, point.mY, point.mZ);
        }

        hull.triangles.reserve(indices.size());
        auto src = reinterpret_cast<const glm::ivec3 *>(indices.data());
        hull.triangles.assign(src, src + indices.size());

        Logf("Adding simple hull, %d points, %d triangles", hull.points.size(), hull.triangles.size());
    }

    void ConvexHullBuilding::BuildConvexHulls(ConvexHullSet *set,
        const Gltf &model,
        const gltf::Mesh &mesh,
        bool decompHull) {
        ZoneScoped;
        for (auto &prim : mesh.primitives) {
            if (!decompHull) {
                // Use points for a single hull without decomposing.
                buildConvexHullForPrimitive(set, model, mesh, prim);
            } else {
                // Break primitive into one or more convex hulls.
                decomposeConvexHullsForPrimitive(set, model, mesh, prim);
            }
        }
    }
} // namespace sp
