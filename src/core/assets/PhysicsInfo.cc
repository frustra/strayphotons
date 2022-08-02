#include "PhysicsInfo.hh"

#include "assets/Asset.hh"

#include <iostream>
#include <picojson/picojson.h>
#include <sstream>

namespace sp {
    PhysicsInfo::PhysicsInfo(const std::string &name, std::shared_ptr<const Asset> asset) : name(name), asset(asset) {
        Assertf(asset, "PhysicsInfo not found: %s", name);

        ZoneScopedN("LoadPhysicsInfo");
        ZonePrintf("%s from %s", name, asset->path.string());

        picojson::value root;
        std::string err = picojson::parse(root, asset->String());
        if (!err.empty()) {
            Errorf("Failed to parse physics info (%s): %s", name, err);
            return;
        }

        if (!root.is<picojson::object>()) {
            Errorf("Unexpected physics info root (%s): %s", name, root.to_str());
            return;
        }

        for (auto &param : root.get<picojson::object>()) {
            HullSettings hull;
            hull.name = name + "." + param.first;

            if (!param.second.is<picojson::object>()) {
                Errorf("Unexpected hull settings (%s): %s", hull.name, param.second.to_str());
                continue;
            }

            for (auto &subParam : param.second.get<picojson::object>()) {
                if (subParam.first == "mesh_index") {
                    if (subParam.second.is<double>()) {
                        hull.meshIndex = (size_t)subParam.second.get<double>();
                    } else {
                        Errorf("Invalid hull mesh_index setting (%s): %s", hull.name, subParam.second.to_str());
                    }
                } else if (subParam.first == "decompose") {
                    if (subParam.second.is<bool>()) {
                        hull.decompose = subParam.second.get<bool>();
                    } else {
                        Errorf("Invalid hull decompose setting (%s): %s", hull.name, subParam.second.to_str());
                    }
                } else if (subParam.first == "shrink_wrap") {
                    if (subParam.second.is<bool>()) {
                        hull.shrinkWrap = subParam.second.get<bool>();
                    } else {
                        Errorf("Invalid hull shrink_wrap setting (%s): %s", hull.name, subParam.second.to_str());
                    }
                } else if (subParam.first == "voxel_resolution") {
                    if (subParam.second.is<double>()) {
                        hull.voxelResolution = (uint32_t)subParam.second.get<double>();
                    } else {
                        Errorf("Invalid hull voxel_resolution setting (%s): %s", hull.name, subParam.second.to_str());
                    }
                } else if (subParam.first == "volume_percent_error") {
                    if (subParam.second.is<double>()) {
                        hull.volumePercentError = subParam.second.get<double>();
                        if (hull.volumePercentError >= 100.0 || hull.volumePercentError <= 0.0) {
                            Errorf("Hull volume_percent_error setting out of range (%s): %u",
                                hull.name,
                                hull.volumePercentError);
                            hull.volumePercentError = 1.0;
                        }
                    } else {
                        Errorf("Invalid hull volume_percent_error setting (%s): %s",
                            hull.name,
                            subParam.second.to_str());
                    }
                } else if (subParam.first == "max_vertices") {
                    if (subParam.second.is<double>()) {
                        hull.maxVertices = (uint32_t)subParam.second.get<double>();
                        if (hull.maxVertices > 255) {
                            Errorf("Hull max_vertices setting exceeds PhysX limit of 255 (%s): %u",
                                hull.name,
                                hull.maxVertices);
                            hull.maxVertices = 255;
                        } else if (hull.maxVertices < 3) {
                            Errorf("Hull max_vertices setting out of range (%s): %u", hull.name, hull.maxVertices);
                            hull.maxVertices = 64;
                        }
                    } else {
                        Errorf("Invalid hull max_vertices setting (%s): %s", hull.name, subParam.second.to_str());
                    }
                } else {
                    Errorf("Unknown hull setting (%s): %s", hull.name, subParam.first);
                }
            }

            hulls.emplace(hull.name, hull);
        }
    }

    const HullSettings &PhysicsInfo::GetHull(const std::string &name) const {
        static const HullSettings defaultHull = {};

        auto it = hulls.find(name);
        if (it != hulls.end()) return it->second;
        return defaultHull;
    }
} // namespace sp
