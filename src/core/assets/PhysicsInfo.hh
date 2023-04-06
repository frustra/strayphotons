#pragma once

#include "core/Common.hh"

#include <unordered_map>

namespace sp {
    class Asset;
    class PhysicsInfo;

    struct HullSettings {
        std::string name; // modelName.meshName

#pragma pack(push, 1)
        // Separate these fields to make them hashable
        // They must be tightly packed because the padding memory is random
        struct Fields {
            size_t meshIndex = 0;
            bool decompose = false;
            bool shrinkWrap = true;
            uint32_t voxelResolution = 400000;
            double volumePercentError = 1.0;
            uint32_t maxVertices = 64;
            uint32_t maxHulls = 64;
        } hull;
#pragma pack(pop)

        std::shared_ptr<const PhysicsInfo> sourceInfo;

        HullSettings() {}
        HullSettings(const std::shared_ptr<const PhysicsInfo> &sourceInfo,
            const std::string &name,
            size_t meshIndex = 0)
            : name(name), sourceInfo(sourceInfo) {
            hull.meshIndex = meshIndex;
        }
    };

    class PhysicsInfo : public NonCopyable {
    public:
        PhysicsInfo(const std::string &modelName, std::shared_ptr<const Asset> asset = nullptr);

        static HullSettings GetHull(const std::shared_ptr<const PhysicsInfo> &source, const std::string &meshName);

        const std::unordered_map<std::string, HullSettings> &GetHulls() const {
            return hulls;
        }

        const std::string modelName;

    private:
        std::shared_ptr<const Asset> asset;
        std::unordered_map<std::string, HullSettings> hulls;
    };
} // namespace sp
