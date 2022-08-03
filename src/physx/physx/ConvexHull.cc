#include "ConvexHull.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "assets/GltfImpl.hh"
#include "assets/PhysicsInfo.hh"
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
        const gltf::Mesh::Primitive &prim,
        const HullSettings &hullSettings) {
        ZoneScoped;
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
        params.m_shrinkWrap = hullSettings.shrinkWrap;
        params.m_resolution = hullSettings.voxelResolution;
        params.m_minimumVolumePercentErrorAllowed = hullSettings.volumePercentError;
        params.m_maxNumVerticesPerCH = hullSettings.maxVertices;
        params.m_maxConvexHulls = hullSettings.maxHulls;

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
        const gltf::Mesh::Primitive &prim,
        const HullSettings &hullSettings) {
        ZoneScoped;
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
        hullImpl.computeConvexHull(points, hullSettings.maxVertices);
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

    std::shared_ptr<ConvexHullSet> hullgen::BuildConvexHulls(const Gltf &model, const HullSettings &hullSettings) {
        ZoneScoped;
        ZoneStr(hullSettings.name);

        if (hullSettings.meshIndex >= model.meshes.size()) {
            Errorf("Physics mesh index %u is out of range: %s", hullSettings.meshIndex, hullSettings.name);
            return nullptr;
        }
        auto &meshOption = model.meshes[hullSettings.meshIndex];
        if (!meshOption) {
            Errorf("Physics mesh index %u is missing: %s", hullSettings.meshIndex, hullSettings.name);
            return nullptr;
        }
        auto &mesh = *meshOption;

        auto set = make_shared<ConvexHullSet>();
        for (auto &prim : mesh.primitives) {
            if (hullSettings.decompose) {
                // Break primitive into one or more convex hulls.
                decomposeConvexHullsForPrimitive(set.get(), model, mesh, prim, hullSettings);
            } else {
                // Use points for a single hull without decomposing.
                buildConvexHullForPrimitive(set.get(), model, mesh, prim, hullSettings);
            }
        }
        return set;
    }

    // Increment if the Collision Cache format ever changes
    const uint32 hullCacheMagic = 0xc044;

#pragma pack(push, 1)
    struct hullCacheHeader {
        uint32_t magicNumber;
        Hash128 modelHash;
        Hash128 settingsHash;
        uint32_t hullCount;
    };

#pragma pack(pop)

    static_assert(sizeof(hullCacheHeader) == 40, "Hull cache header size changed unexpectedly");

    /*std::shared_ptr<ConvexHullSet> hullgen::LoadCollisionCache(const Gltf &model, const HullSettings &hullSettings) {
        ZoneScoped;
        ZoneStr(hullSettings.name);

        Assertf(hullSettings.meshIndex < model.meshes.size(),
            "Physics mesh index is out of range: %s",
            hullSettings.name);
        auto &mesh = model.meshes[hullSettings.meshIndex];
        Assertf(mesh, "Physics mesh is undefined: %s index %u", hullSettings.name, hullSettings.meshIndex);

        std::string path = "cache/collision/" + hullSettings.name;

        auto asset = GAssets.Load(path)->Get();
        if (!asset) {
            Errorf("Physics collision cache missing for hull: %s", hullSettings.name);
            return nullptr;
        }

        auto buf = asset->Buffer();
        if (buf.size() < sizeof(hullCacheHeader)) {
            Errorf("Physics collision cache is too short: %s", hullSettings.name);
            return nullptr;
        }

        auto *header = reinterpret_cast<const hullCacheHeader *>(buf.data());
        if (header->magicNumber != hullCacheMagic) {
            Logf("Ignoring outdated collision cache format for %s", hullSettings.name);
            return nullptr;
        }

        if (!model.asset || header->modelHash != model.asset->Hash()) {
            Logf("Ignoring outdated collision cache for %s", path);
            return nullptr;
        }

        // TODO: Hash the hull settings and compare them

        auto hullSet = make_shared<ConvexHullSet>();
        hullSet->hulls.reserve(header->hullCount);

        for (uint32_t i = 0; i < header->hullCount; i++) {
            auto &hull = hullSet->hulls.emplace_back();

            uint32_t pointCount, triangleCount;
            in.read((char *)&pointCount, sizeof(uint32_t));
            in.read((char *)&triangleCount, sizeof(uint32_t));

            hull.points.resize(pointCount);
            hull.triangles.resize(triangleCount);

            in.read((char *)hull.points.data(), hull.points.size() * sizeof(glm::vec3));
            in.read((char *)hull.triangles.data(), hull.triangles.size() * sizeof(glm::ivec3));
        }

        return hullSet;
    }

    void hullgen::SaveCollisionCache(const Gltf &model,
        size_t meshIndex,
        const ConvexHullSet &set,
        bool decomposeHull) {
        ZoneScoped;
        ZonePrintf("%s.%u", model.name, meshIndex);
        Assertf(meshIndex < model.meshes.size(), "LoadCollisionCache %s mesh %u out of range", model.name, meshIndex);
        std::ofstream out;

        std::string path = "cache/collision/" + model.name + "." + std::to_string(meshIndex);
        if (decomposeHull) path += "-decompose";

        if (GAssets.OutputStream(path, out)) {
            out.write((char *)&hullCacheMagic, 4);

            Hash128 hash = model.asset->Hash();
            out.write((char *)hash.data(), sizeof(hash));

            int32 hullCount = set.hulls.size();
            out.write((char *)&hullCount, 4);

            for (auto hull : set.hulls) {
                Assert(hull.points.size() < UINT32_MAX, "hull point count overflows uint32");
                Assert(hull.triangles.size() < UINT32_MAX, "hull triangle count overflows uint32");
                uint32_t pointCount = hull.points.size();
                uint32_t triangleCount = hull.triangles.size();
                out.write((char *)&pointCount, sizeof(uint32_t));
                out.write((char *)&triangleCount, sizeof(uint32_t));
                out.write((char *)hull.points.data(), hull.points.size() * sizeof(glm::vec3));
                out.write((char *)hull.triangles.data(), hull.triangles.size() * sizeof(glm::ivec3));
            }

            out.close();
        }
    }*/
} // namespace sp
