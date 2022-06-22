#include "ConvexHull.hh"

#include "assets/Gltf.hh"
#include "assets/GltfImpl.hh"
#include "core/Logging.hh"

#include <v-hacd/VHACD_Lib/inc/btConvexHullComputer.h>
#include <v-hacd/VHACD_Lib/inc/vhacdICHull.h>
#include <v-hacd/VHACD_Lib/inc/vhacdVector.h>
#include <v-hacd/VHACD_Lib/public/VHACD.h>

#define GLM_FORCE_CXX11
#include <glm/gtx/hash.hpp>
#include <unordered_set>

namespace sp {
    class VHACDCallback : public VHACD::IVHACD::IUserCallback {
    public:
        VHACDCallback() {}
        ~VHACDCallback() {}

        void Update(const double overallProgress,
            const double stageProgress,
            const double operationProgress,
            const char *const stage,
            const char *const operation) {
            Logf("VHACD %d (%s) %s", (int)(overallProgress + 0.5), stage, operation);
        };
    };

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
        params.m_projectHullVertices = false;
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
            if (ihull.m_nPoints < 3 || ihull.m_nTriangles == 0) continue;

            ConvexHull hull;
            hull.points.resize(ihull.m_nPoints);

            for (uint32 j = 0; j < ihull.m_nPoints; j++) {
                hull.points[j] = glm::vec3(ihull.m_points[j * 3], ihull.m_points[j * 3 + 1], ihull.m_points[j * 3 + 2]);
            }

            auto src = reinterpret_cast<glm::ivec3 *>(ihull.m_triangles);
            hull.triangles.assign(src, src + ihull.m_nTriangles);

            set->hulls.push_back(hull);
            Logf("Adding VHACD hull, %d points, %d triangles", hull.points.size(), hull.triangles.size());
        }

        interfaceVHACD->Clean();
        interfaceVHACD->Release();
    }

    void copyVhacdManifoldMeshToConvexHull(ConvexHull &hull, VHACD::TMMesh &mesh) {
        const size_t nT = mesh.GetNTriangles();
        const size_t nV = mesh.GetNVertices();

        hull.points.resize(nV);
        for (size_t v = 0; v < nV; v++) {
            auto &pos = mesh.GetVertices().GetData().m_pos;
            hull.points[v] = glm::vec3(pos[0], pos[1], pos[2]);
            mesh.GetVertices().GetData().m_id = v;
            mesh.GetVertices().Next();
        }

        hull.triangles.resize(nT);
        for (size_t f = 0; f < nT; f++) {
            auto &currentTriangle = mesh.GetTriangles().GetData();
            hull.triangles[f].x = static_cast<int>(currentTriangle.m_vertices[0]->GetData().m_id);
            hull.triangles[f].y = static_cast<int>(currentTriangle.m_vertices[1]->GetData().m_id);
            hull.triangles[f].z = static_cast<int>(currentTriangle.m_vertices[2]->GetData().m_id);
            mesh.GetTriangles().Next();
        }
    }

    void buildConvexHullForPrimitive(ConvexHullSet *set,
        const Gltf &model,
        const gltf::Mesh &mesh,
        const gltf::Mesh::Primitive &prim) {
        ZoneScoped;
        set->decomposed = false;

        std::unordered_set<uint32_t> visitedIndexes;
        std::vector<glm::vec3> points;
        points.reserve(prim.positionBuffer.Count());

        for (size_t i = 0; i < prim.indexBuffer.Count(); i++) {
            uint32_t index = prim.indexBuffer.Read(i);
            if (visitedIndexes.count(index) || index >= prim.positionBuffer.Count()) continue;

            points.emplace_back(prim.positionBuffer.Read(index));
            visitedIndexes.insert(index);
        }

        btConvexHullComputer chc;
        chc.compute(reinterpret_cast<const float *>(points.data()), sizeof(glm::vec3), points.size(), -1, -1);

        VHACD::ICHull icc;

        for (int i = 0; i < chc.vertices.size(); i++) {
            auto &v = chc.vertices[i];
            VHACD::Vec3<double> point(v[0], v[1], v[2]);
            icc.AddPoint(point);
        }

        icc.Process(128, 0);

        ConvexHull hull;
        copyVhacdManifoldMeshToConvexHull(hull, icc.GetMesh());
        if (hull.points.size() < 3 || hull.triangles.size() == 0) return;
        set->hulls.push_back(hull);
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
