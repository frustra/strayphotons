/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "VoxelData.hh"

#include "assets/JsonHelpers.hh"
#include "strayphotons/HeapString.hh"
#include "strayphotons/Utility.hh"

#include <libbase64.h>
#include <picojson.h>

namespace ecs {
    template<>
    bool StructMetadata::Load<VoxelData>(VoxelData &data, const picojson::value &src) {
        if (!src.is<picojson::object>()) {
            Errorf("Invalid voxel data: %s", src.to_str());
            return false;
        }

        for (auto &property : src.get<picojson::object>()) {
            if (property.first == "data") {
                if (property.second.is<std::string>()) {
                    const std::string &input = property.second.get<std::string>();
                    if (sp::starts_with(input, "base64:")) {
                        std::string_view inputBase64 = std::string_view(input).substr(7);
                        data.data.resize((inputBase64.size() * 3 + 3) / 4);
                        size_t bufSize = data.data.size();
                        int result = base64_decode(inputBase64.data(),
                            inputBase64.size(),
                            reinterpret_cast<char *>(data.data.data()),
                            &bufSize,
                            0);
                        if (result) {
                            data.data.resize(std::min(data.data.size(), bufSize));
                        } else {
                            Errorf("VoxelData invalid base64 data: %s", inputBase64);
                            return false;
                        }
                    } else {
                        Errorf("VoxelData invalid data: %s", input);
                        return false;
                    }
                } else {
                    Errorf("VoxelData invalid data: %s", property.second.to_str());
                    return false;
                }
            }
        }
        return true;
    }

    template<>
    void StructMetadata::Save<VoxelData>(const EntityScope &scope,
        picojson::value &dst,
        const VoxelData &src,
        const VoxelData *def) {
        if (src.data.empty()) return;

        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &obj = dst.get<picojson::object>();

        sp::HeapString output;
        output.resize((src.data.size() * 4 + 2) / 3);
        size_t strLength = output.size();
        base64_encode(reinterpret_cast<const char *>(src.data.data()), src.data.size(), output.data(), &strLength, 0);
        output.resize(std::min(output.size(), strLength));
        if (!output.empty()) sp::json::Save(scope, obj["data"], "base64:" + output);
    }
} // namespace ecs
