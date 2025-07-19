/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "assets/Asset.hh"
#include "assets/AssetManager.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <cmath>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace sp::scripts {
    using namespace ecs;

    template<typename Vec>
    void SplitStr(std::string_view str, char delim, Vec &out) {
        out.clear();
        size_t lastDelim = 0;
        size_t i = str.find(delim);
        while (i != std::string_view::npos) {
            out.emplace_back(str.substr(lastDelim, i - lastDelim));
            lastDelim = i + 1;
            i = str.find(delim, lastDelim);
        }
        out.emplace_back(str.substr(lastDelim));
    }

    std::string_view StripQuotes(std::string_view str) {
        if (starts_with(str, "\"") && ends_with(str, "\"")) {
            return str.substr(1, str.size() - 2);
        }
        return str;
    }

    double ParseNumber(std::string_view str) {
        if (sp::is_float(str)) {
            return std::stod(std::string(str));
        }
        return NAN;
    }

    struct ColumnData {
        std::string name;
        std::string unit;
        double min, max;
        size_t sampleRate;
        std::vector<std::pair<size_t, double>> data;

        ColumnData(std::string_view header, size_t reservedLines) {
            InlineVector<std::string_view, 5> parts;
            SplitStr(header, '|', parts);
            Assertf(parts.size() == 5, "Invalid column header: %s", std::string(header));
            name = StripQuotes(parts[0]);
            unit = StripQuotes(parts[1]);
            min = ParseNumber(parts[2]);
            max = ParseNumber(parts[3]);
            sampleRate = (size_t)ParseNumber(parts[4]);
            data.reserve(reservedLines);
        }

        double SampleTimestamp(size_t intervalMs) {
            auto it = std::lower_bound(data.begin(), data.end(), intervalMs, [](auto &entry, size_t intervalMs) {
                return entry.first < intervalMs;
            });
            if (it != data.end()) {
                return it->second;
            } else {
                return NAN;
            }
        }
    };

    struct CSVVisualizer {
        std::string filename;
        std::string loaded;
        AsyncPtr<Asset> assetPtr;
        std::vector<ColumnData> columns;
        SignalRef loadingRef, accelXRef, accelYRef, accelZRef, xRef, yRef, zRef;
        std::vector<SignalRef> outputs;
        size_t currentTimeNs = 0;
        glm::vec3 lastPosition;
        glm::vec3 lastDir = glm::vec3(0, 0, -1);

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!loadingRef) {
                loadingRef = SignalRef(ent, "loading");
                accelXRef = SignalRef(ent, "accel_x");
                accelYRef = SignalRef(ent, "accel_y");
                accelZRef = SignalRef(ent, "accel_z");
                xRef = SignalRef(ent, "x");
                yRef = SignalRef(ent, "y");
                zRef = SignalRef(ent, "z");
            }

            if (!assetPtr || filename != loaded) {
                columns.clear();
                for (auto &ref : outputs) {
                    ref.ClearValue(lock);
                }
                outputs.clear();
                loadingRef.SetValue(lock, 1);
                assetPtr = Assets().Load(filename, AssetType::External);
                loaded = filename;
            }
            if (!assetPtr->Ready()) return;
            auto asset = assetPtr->Get();
            if (!asset) return;
            if (columns.empty()) {
                std::string_view dataStr((const char *)asset->BufferPtr(), asset->BufferSize());
                std::vector<std::string_view> lines;
                SplitStr(dataStr, '\n', lines);
                std::vector<std::string_view> columnNames;
                SplitStr(lines.front(), ',', columnNames);
                for (auto &col : columnNames) {
                    auto &colData = columns.emplace_back(col, lines.size() - 1);
                    outputs.emplace_back(ent, colData.name);
                }
                std::vector<std::string_view> values;
                for (size_t i = 1; i < lines.size(); i++) {
                    SplitStr(lines[i], ',', values);
                    size_t intervalCol = (size_t)ParseNumber(values.front());
                    for (size_t c = 0; c < values.size(); c++) {
                        double num = ParseNumber(values[c]);
                        if (!std::isnan(num)) {
                            columns[c].data.emplace_back(intervalCol, num);
                        }
                    }
                }
                loadingRef.ClearValue(lock);
                currentTimeNs = (size_t)columns.front().data.front().second * 1e6;
            } else {
                for (size_t i = 0; i < columns.size(); i++) {
                    double value = columns[i].SampleTimestamp(currentTimeNs / 1e6);
                    if (std::isnan(value)) {
                        outputs[i].ClearValue(lock);
                    } else {
                        outputs[i].SetValue(lock, value);
                    }
                }
            }
            if (ent.Has<ecs::TransformTree>(lock)) {
                auto &transformTree = ent.Get<ecs::TransformTree>(lock);
                transformTree.pose.SetPosition(
                    glm::vec3(xRef.GetSignal(lock), yRef.GetSignal(lock), zRef.GetSignal(lock)));

                glm::vec3 direction = lastDir;
                auto deltaPos = transformTree.pose.GetPosition() - lastPosition;
                if (glm::length2(deltaPos) > 0.001) {
                    direction = -glm::normalize(deltaPos);
                }
                auto oldDir = glm::quatLookAt(lastDir, glm::vec3(0, 1, 0));
                auto newDir = glm::quatLookAt(direction, glm::vec3(0, 1, 0));
                newDir = glm::normalize(glm::slerp(oldDir, newDir, 0.2f));
                transformTree.pose.SetRotation(newDir);

                lastPosition = transformTree.pose.GetPosition();
                lastDir = newDir * glm::vec3(0, 0, -1);

                glm::vec3 down(accelXRef.GetSignal(lock), accelYRef.GetSignal(lock), accelZRef.GetSignal(lock));
                if (!glm::all(glm::epsilonEqual(down, glm::vec3(0), 0.001f))) {
                    down = glm::normalize(down);
                } else {
                    down = glm::vec3(0, -1, 0);
                }
                transformTree.pose.Rotate(glm::rotation(glm::vec3(0, -1, 0), down));
            }
            currentTimeNs += interval.count();
        }
    };
    StructMetadata MetadataCSVVisualizer(typeid(CSVVisualizer),
        "CSVVisualizer",
        "",
        StructField::New("filename", &CSVVisualizer::filename),
        StructField::New("current_time_ns", &CSVVisualizer::currentTimeNs));
    InternalScript<CSVVisualizer> csvVisualizer("csv_visualizer", MetadataCSVVisualizer);
} // namespace sp::scripts
