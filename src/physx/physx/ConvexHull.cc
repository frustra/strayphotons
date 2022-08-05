#include "ConvexHull.hh"

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "assets/GltfImpl.hh"
#include "assets/PhysicsInfo.hh"
#include "core/Logging.hh"

#include <PxPhysicsAPI.h>
#include <cstdlib>
#include <fstream>
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

    std::shared_ptr<physx::PxConvexMesh> createPhysxMesh(physx::PxCooking &cooking,
        physx::PxPhysics &physics,
        const std::vector<VHACD::Vertex> &inputPoints) {

        std::vector<glm::vec3> points;
        points.reserve(inputPoints.size());
        for (auto &point : inputPoints) {
            points.emplace_back(point.mX, point.mY, point.mZ);
        }

        physx::PxConvexMeshDesc convexDesc;
        convexDesc.points.count = points.size();
        convexDesc.points.stride = sizeof(*points.data());
        convexDesc.points.data = reinterpret_cast<const float *>(points.data());
        convexDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX;

        auto *pxMesh = cooking.createConvexMesh(convexDesc, physics.getPhysicsInsertionCallback());
        if (!pxMesh) {
            Errorf("Failed to cook PhysX hull for %u points", inputPoints.size());
            return nullptr;
        }

        return std::shared_ptr<physx::PxConvexMesh>(pxMesh, [](physx::PxConvexMesh *ptr) {
            ptr->release();
        });
    }

    void decomposeConvexHullsForPrimitive(physx::PxCooking &cooking,
        physx::PxPhysics &physics,
        ConvexHullSet *set,
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
            if (ihull.m_points.size() < 3) continue;

            auto pxMesh = createPhysxMesh(cooking, physics, ihull.m_points);
            if (pxMesh) {
                Logf("Adding VHACD hull, %d points, %d triangles", ihull.m_points.size(), ihull.m_triangles.size());
                set->hulls.emplace_back(pxMesh);
            }
        }

        interfaceVHACD->Clean();
        interfaceVHACD->Release();
    }

    void buildConvexHullForPrimitive(physx::PxCooking &cooking,
        physx::PxPhysics &physics,
        ConvexHullSet *set,
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
        if (vertices.size() < 3) return;

        auto pxMesh = createPhysxMesh(cooking, physics, vertices);
        if (pxMesh) {
            Logf("Adding simple hull, %d points, %d triangles", vertices.size(), hullImpl.getIndices().size());
            set->hulls.emplace_back(pxMesh);
        }
    }

    std::shared_ptr<ConvexHullSet> hullgen::BuildConvexHulls(physx::PxCooking &cooking,
        physx::PxPhysics &physics,
        const Gltf &model,
        const HullSettings &hullSettings) {
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
                decomposeConvexHullsForPrimitive(cooking, physics, set.get(), model, mesh, prim, hullSettings);
            } else {
                // Use points for a single hull without decomposing.
                buildConvexHullForPrimitive(cooking, physics, set.get(), model, mesh, prim, hullSettings);
            }
        }
        return set;
    }

    // Increment if the Collision Cache format ever changes
    const uint32 hullCacheMagic = 0xc044;

#pragma pack(push, 1)
    struct hullCacheHeader {
        uint32_t magicNumber = hullCacheMagic;
        Hash128 modelHash;
        Hash128 settingsHash;
        uint32_t bufferSize = 0;
    };

#pragma pack(pop)

    static_assert(sizeof(hullCacheHeader) == 40, "Hull cache header size changed unexpectedly");

    std::shared_ptr<ConvexHullSet> hullgen::LoadCollisionCache(physx::PxSerializationRegistry &registry,
        const Gltf &model,
        const HullSettings &hullSettings) {
        ZoneScoped;
        ZoneStr(hullSettings.name);

        Assertf(hullSettings.meshIndex < model.meshes.size(),
            "Physics mesh index is out of range: %s",
            hullSettings.name);
        auto &mesh = model.meshes[hullSettings.meshIndex];
        Assertf(mesh, "Physics mesh is undefined: %s index %u", hullSettings.name, hullSettings.meshIndex);

        auto asset = GAssets.Load("cache/collision/" + hullSettings.name)->Get();
        if (!asset) {
            Errorf("Physics collision cache missing for hull: %s", hullSettings.name);
            return nullptr;
        }

        auto buf = asset->Buffer();
        if (buf.size() < sizeof(hullCacheHeader)) {
            Errorf("Physics collision cache is corrupt: %s", hullSettings.name);
            return nullptr;
        }

        auto *header = reinterpret_cast<const hullCacheHeader *>(buf.data());
        if (header->magicNumber != hullCacheMagic) {
            Logf("Ignoring outdated collision cache format for %s", hullSettings.name);
            return nullptr;
        }

        if (!model.asset || header->modelHash != model.asset->Hash()) {
            Logf("Ignoring outdated collision cache for %s", hullSettings.name);
            return nullptr;
        }

        // TODO: Hash the hull settings and compare them

        if (buf.size() - sizeof(hullCacheHeader) < header->bufferSize) {
            Errorf("Physics collision cache is corrupt: %s", hullSettings.name);
            return nullptr;
        }

        auto hullSet = make_shared<ConvexHullSet>();
        hullSet->collectionBuffer.resize(header->bufferSize + 128);

        // Copy the serialization data into 128-byte aligned memory for PhysX
        size_t remaining = hullSet->collectionBuffer.size();
        void *alignedMemory = hullSet->collectionBuffer.data();
        std::align(128, header->bufferSize, alignedMemory, remaining);
        std::memcpy(alignedMemory, buf.data() + sizeof(hullCacheHeader), header->bufferSize);

        auto *collection = physx::PxSerialization::createCollectionFromBinary(alignedMemory, registry);
        if (!collection) {
            Errorf("Failed to load physx serialization: %s", hullSettings.name);
            return nullptr;
        }
        hullSet->collection = std::shared_ptr<physx::PxCollection>(collection, [](physx::PxCollection *ptr) {
            ptr->release();
        });

        hullSet->hulls.reserve(collection->getNbObjects());
        for (uint32_t i = 0; i < collection->getNbObjects(); i++) {
            physx::PxBase &object = collection->getObject(i);
            auto pxMesh = object.is<physx::PxConvexMesh>();
            if (!pxMesh) {
                object.release();
                continue;
            }

            hullSet->hulls.emplace_back(pxMesh, [i, name = hullSettings.name](physx::PxConvexMesh *ptr) {
                ptr->release();
            });
        }
        return hullSet;
    }

    void hullgen::SaveCollisionCache(physx::PxSerializationRegistry &registry,
        const Gltf &model,
        const HullSettings &hullSettings,
        const ConvexHullSet &set) {
        ZoneScoped;
        ZoneStr(hullSettings.name);

        Assertf(hullSettings.meshIndex < model.meshes.size(),
            "SaveCollisionCache mesh index is out of range: %s",
            hullSettings.name);
        auto &mesh = model.meshes[hullSettings.meshIndex];
        Assertf(mesh, "SaveCollisionCache mesh is undefined: %s index %u", hullSettings.name, hullSettings.meshIndex);

        auto *collection = PxCreateCollection();
        for (auto hull : set.hulls) {
            if (!hull) continue;
            collection->add(*hull);
        }
        physx::PxSerialization::complete(*collection, registry);

        physx::PxDefaultMemoryOutputStream buf;
        if (!physx::PxSerialization::serializeCollectionToBinary(buf, *collection, registry)) {
            Errorf("Failed to serialize convex hull set: %s", hullSettings.name);
            return;
        }

        std::ofstream out;
        if (GAssets.OutputStream("cache/collision/" + hullSettings.name, out)) {
            hullCacheHeader header = {};
            header.modelHash = model.asset->Hash();
            header.settingsHash = Hash128(); // TODO: Fill this in
            header.bufferSize = buf.getSize();

            out.write(reinterpret_cast<const char *>(&header), sizeof(header));
            out.write(reinterpret_cast<const char *>(buf.getData()), buf.getSize());

            out.close();
        }
    }
} // namespace sp
