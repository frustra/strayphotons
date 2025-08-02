/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "assets/AssetManager.hh"
#include "common/Common.hh"
#include "common/Hashing.hh"

#include <robin_hood.h>

namespace sp {
    class Asset;
    class PhysicsInfo;

    struct HullSettings {
        AssetName name; // modelName.meshName

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
        HullSettings(const std::shared_ptr<const PhysicsInfo> &sourceInfo, std::string_view name, size_t meshIndex = 0)
            : name(name), sourceInfo(sourceInfo) {
            hull.meshIndex = meshIndex;
        }
    };

    class PhysicsInfo : public NonCopyable {
    public:
        PhysicsInfo(std::string_view modelName, std::shared_ptr<const Asset> asset = nullptr);

        static HullSettings GetHull(const std::shared_ptr<const PhysicsInfo> &source, std::string_view meshName);

        const robin_hood::unordered_map<AssetName, HullSettings, StringHash, StringEqual> &GetHulls() const {
            return hulls;
        }

        const AssetName modelName;

    private:
        std::shared_ptr<const Asset> asset;
        robin_hood::unordered_map<AssetName, HullSettings, StringHash, StringEqual> hulls;
    };
} // namespace sp
