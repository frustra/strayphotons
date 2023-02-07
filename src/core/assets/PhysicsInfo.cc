#include "PhysicsInfo.hh"

#include "assets/Asset.hh"

#include <cstdlib>
#include <iostream>
#include <picojson/picojson.h>
#include <sstream>

namespace sp {
    PhysicsInfo::PhysicsInfo(const std::string &modelName, std::shared_ptr<const Asset> asset)
        : modelName(modelName), asset(asset) {
        Assertf(!modelName.empty(), "PhysicsInfo is missing model name");

        if (!asset) return;

        ZoneScopedN("LoadPhysicsInfo");
        ZonePrintf("%s from %s", modelName, asset->path.string());

        picojson::value root;
        std::string err = picojson::parse(root, asset->String());
        if (!err.empty()) {
            Errorf("Failed to parse physics info (%s): %s", modelName, err);
            return;
        }

        if (!root.is<picojson::object>()) {
            Errorf("Unexpected physics info root (%s): %s", modelName, root.to_str());
            return;
        }

        for (auto &param : root.get<picojson::object>()) {
            HullSettings settings;
            settings.name = modelName + "." + param.first;
            auto &hull = settings.hull;

            if (!param.second.is<picojson::object>()) {
                Errorf("Unexpected hull settings (%s): %s", settings.name, param.second.to_str());
                continue;
            }

            for (auto &subParam : param.second.get<picojson::object>()) {
                if (subParam.first == "mesh_index") {
                    if (subParam.second.is<double>()) {
                        hull.meshIndex = (size_t)subParam.second.get<double>();
                    } else {
                        Errorf("Invalid hull mesh_index setting (%s): %s", settings.name, subParam.second.to_str());
                    }
                } else if (subParam.first == "decompose") {
                    if (subParam.second.is<bool>()) {
                        hull.decompose = subParam.second.get<bool>();
                    } else {
                        Errorf("Invalid hull decompose setting (%s): %s", settings.name, subParam.second.to_str());
                    }
                } else if (subParam.first == "shrink_wrap") {
                    if (subParam.second.is<bool>()) {
                        hull.shrinkWrap = subParam.second.get<bool>();
                    } else {
                        Errorf("Invalid hull shrink_wrap setting (%s): %s", settings.name, subParam.second.to_str());
                    }
                } else if (subParam.first == "voxel_resolution") {
                    if (subParam.second.is<double>()) {
                        hull.voxelResolution = (uint32_t)subParam.second.get<double>();
                    } else {
                        Errorf("Invalid hull voxel_resolution setting (%s): %s",
                            settings.name,
                            subParam.second.to_str());
                    }
                } else if (subParam.first == "volume_percent_error") {
                    if (subParam.second.is<double>()) {
                        hull.volumePercentError = subParam.second.get<double>();
                        if (hull.volumePercentError >= 100.0 || hull.volumePercentError <= 0.0) {
                            Errorf("Hull volume_percent_error setting out of range (%s): %u",
                                settings.name,
                                hull.volumePercentError);
                            hull.volumePercentError = 1.0;
                        }
                    } else {
                        Errorf("Invalid hull volume_percent_error setting (%s): %s",
                            settings.name,
                            subParam.second.to_str());
                    }
                } else if (subParam.first == "max_vertices") {
                    if (subParam.second.is<double>()) {
                        hull.maxVertices = (uint32_t)subParam.second.get<double>();
                        if (hull.maxVertices > 255) {
                            Errorf("Hull max_vertices setting exceeds PhysX limit of 255 (%s): %u",
                                settings.name,
                                hull.maxVertices);
                            hull.maxVertices = 255;
                        } else if (hull.maxVertices < 3) {
                            Errorf("Hull max_vertices setting out of range (%s): %u", settings.name, hull.maxVertices);
                            hull.maxVertices = 64;
                        }
                    } else {
                        Errorf("Invalid hull max_vertices setting (%s): %s", settings.name, subParam.second.to_str());
                    }
                } else if (subParam.first == "max_hulls") {
                    if (subParam.second.is<double>()) {
                        hull.maxHulls = (uint32_t)subParam.second.get<double>();
                        if (hull.maxHulls < 1) {
                            Errorf("Hull max_hulls setting out of range (%s): %u", settings.name, hull.maxHulls);
                            hull.maxHulls = 64;
                        }
                    } else {
                        Errorf("Invalid hull max_hulls setting (%s): %s", settings.name, subParam.second.to_str());
                    }
                } else {
                    Errorf("Unknown hull setting (%s): %s", settings.name, subParam.first);
                }
            }

            hulls.emplace(param.first, settings);
        }
    }

    HullSettings PhysicsInfo::GetHull(const std::shared_ptr<const PhysicsInfo> &source, const std::string &meshName) {
        Assertf(source, "PhysicsInfo::GetHull called with null source");

        auto it = source->hulls.find(meshName);
        if (it != source->hulls.end()) return it->second;
        if (starts_with(meshName, "convex")) {
            auto end = std::find_if(meshName.begin() + 6, meshName.end(), [](char ch) {
                return !std::isdigit(ch);
            });
            if (end == meshName.end()) {
                size_t meshIndex = strtoull(meshName.c_str() + 6, nullptr, 10); // Defaults to 0 on failure
                return HullSettings(source, source->modelName + "." + meshName, meshIndex);
            } else {
                Warnf("Missing physics hull, defaulting to convex: %s.%s", source->modelName, meshName);
            }
        } else {
            Warnf("Missing physics hull, defaulting to convex: %s.%s", source->modelName, meshName);
        }
        return {};
    }
} // namespace sp
