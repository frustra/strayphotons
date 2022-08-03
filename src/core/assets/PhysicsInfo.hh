#pragma once

#include "core/Common.hh"

namespace sp {
    class Asset;

    struct HullSettings {
        std::string name; // modelName.meshName
        size_t meshIndex = 0;
        bool decompose = false;
        bool shrinkWrap = true;
        uint32_t voxelResolution = 400000;
        double volumePercentError = 1.0;
        uint32_t maxVertices = 64;
        uint32_t maxHulls = 64;

        HullSettings() {}
        HullSettings(const std::string &name, size_t meshIndex = 0) : name(name), meshIndex(meshIndex) {}
    };

    class PhysicsInfo : public NonCopyable {
    public:
        PhysicsInfo(const std::string &modelName, std::shared_ptr<const Asset> asset = nullptr);

        HullSettings GetHull(const std::string &meshName) const;

        const std::unordered_map<std::string, HullSettings> &GetHulls() const {
            return hulls;
        }

        const std::string modelName;

    private:
        std::shared_ptr<const Asset> asset;
        std::unordered_map<std::string, HullSettings> hulls;
    };
} // namespace sp
