/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "shared_events.h"

#include <c_abi/Tecs.hh>
#include <c_abi/strayphotons_ecs_c_abi_entity_gen.h>
#include <c_abi/strayphotons_ecs_c_abi_lock_gen.h>
#include <common/Async.hh>
#include <common/DispatchQueue.hh>
#include <common/InlineVector.hh>
#include <common/Logging.hh>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <strayphotons/components.h>

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
    if (sp::starts_with(str, "\"") && sp::ends_with(str, "\"")) {
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

struct ColumnData : public ColumnMetadata {
    std::vector<std::pair<size_t, double>> data;

    ColumnData(std::string_view header, size_t reservedLines) {
        sp::InlineVector<std::string_view, 5> parts;
        SplitStr(header, '|', parts);
        Assertf(parts.size() == 5, "Invalid column header: %s", std::string(header));
        name = StripQuotes(parts[0]);
        unit = StripQuotes(parts[1]);
        // min = ParseNumber(parts[2]);
        // max = ParseNumber(parts[3]);
        min = std::numeric_limits<double>::max();
        max = std::numeric_limits<double>::min();
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

    uint32_t SampleRange(size_t start, size_t end, size_t sampleOffset, size_t resolution, ColumnRange &output) {
        auto getSampleStart = [&](size_t i) {
            return i * (end - start) / resolution + start;
        };
        output.columnIndex = columnIndex;
        output.sampleOffset = sampleOffset;
        size_t rangeStart = getSampleStart(sampleOffset);
        size_t rangeEnd = rangeStart;
        output.sampleCount = 0;
        auto it = std::lower_bound(data.begin(), data.end(), rangeStart, [](auto &entry, size_t intervalMs) {
            return entry.first < intervalMs;
        });
        while (output.sampleCount < output.size() && rangeEnd < end) {
            rangeEnd = getSampleStart(sampleOffset + output.sampleCount + 1);
            if (rangeEnd > end) rangeEnd = end;
            while ((it == data.end() || it->first >= rangeEnd) && output.sampleCount < output.size()) {
                // Insert extra values to display previous value
                if (it != data.begin()) {
                    output[output.sampleCount] = {(float)(it - 1)->second, (float)(it - 1)->second};
                } else {
                    output[output.sampleCount] = {NAN, NAN};
                }
                output.sampleCount++;
                rangeEnd = getSampleStart(sampleOffset + output.sampleCount + 1);
                if (rangeEnd > end) rangeEnd = end;
            }
            if (output.sampleCount >= output.size()) break;
            auto &out = output[output.sampleCount];
            out.min = NAN;
            out.max = NAN;
            while (it != data.end() && it->first < rangeEnd) {
                float sample = (float)it->second;
                if (std::isnan(out.min) || sample < out.min) out.min = sample;
                if (std::isnan(out.max) || sample > out.max) out.max = sample;
                it++;
            }
            output.sampleCount++;
        }
        return output.sampleCount;
    }
};

struct CSVVisualizer {
    sp::InlineString<255> filename;
    sp::InlineString<255> loaded;
    sp_entity_ref_t entityRef = {};
    sp_signal_ref_t loadingRef = {}, accelXRef = {}, accelYRef = {}, accelZRef = {}, xRef = {}, yRef = {}, zRef = {};
    std::vector<sp_signal_ref_t> outputs;
    uint64_t currentTimeNs = 0;
    glm::vec3 lastPosition;
    glm::vec3 lastDir = glm::vec3(0, 0, -1);

    std::optional<sp::DispatchQueue> workQueue;
    sp::AsyncPtr<std::vector<char>> assetPtr;
    sp::AsyncPtr<std::vector<ColumnData>> columnsPtr;
    std::atomic_int32_t loadingProgress = -1;

    CSVVisualizer() {}

    void HandleEvents(sp_script_state_t *state, tecs_lock_t *lock, tecs_entity_t ent) {
        sp_event_t *event;
        while ((event = sp_script_state_poll_event(state, lock))) {
            if (strcmp(event->name, "/csv/get_metadata") == 0) {
                if (event->data.type != SP_EVENT_DATA_TYPE_UINT) {
                    Warnf("Invalid event type: %s %s", event->name, event->data.type);
                    continue;
                }
                uint32_t col = event->data.ui;
                if (!columnsPtr || !columnsPtr->Ready()) {
                    sp_event_t eventOut = {"/csv/loading_progress", ent, {SP_EVENT_DATA_TYPE_INT}};
                    eventOut.data.i = loadingProgress.load();
                    sp_event_send(lock, event->source, &eventOut);
                } else {
                    auto columns = columnsPtr->Get();
                    if (columns && col < columns->size()) {
                        sp_event_t eventOut = {"/csv/column_metadata", ent, {SP_EVENT_DATA_TYPE_BYTES}};
                        static_assert(sizeof(ColumnMetadata) <= sizeof(event_bytes_t));
                        std::copy_n((const uint8_t *)(const ColumnMetadata *)&(*columns)[col],
                            sizeof(ColumnMetadata),
                            &eventOut.data.bytes[0]);
                        sp_event_send(lock, event->source, &eventOut);
                    } else {
                        sp_event_t eventOut = {"/csv/column_metadata", ent, {SP_EVENT_DATA_TYPE_BOOL}};
                        eventOut.data.b = true;
                        sp_event_send(lock, event->source, &eventOut);
                    }
                }
            } else if (strcmp(event->name, "/csv/query_range") == 0) {
                if (event->data.type != SP_EVENT_DATA_TYPE_VEC4) {
                    Warnf("Invalid event type: %s %s", event->name, event->data.type);
                    continue;
                }
                uint32_t columnIndex = (uint32_t)event->data.vec4.v[0];
                if (!columnsPtr || !columnsPtr->Ready()) {
                    sp_event_t eventOut = {"/csv/loading_progress", ent, {SP_EVENT_DATA_TYPE_INT}};
                    eventOut.data.i = loadingProgress.load();
                    sp_event_send(lock, event->source, &eventOut);
                } else {
                    auto columns = columnsPtr->Get();
                    if (columns && columnIndex < columns->size()) {
                        size_t resolution = event->data.vec4.v[1];
                        size_t rangeStart = event->data.vec4.v[2];
                        size_t rangeSize = event->data.vec4.v[3];
                        size_t rangeEnd = rangeStart + rangeSize;

                        size_t offset = 9;
                        ColumnRange rangeOutput = {};
                        while (offset < resolution) {
                            uint32_t count = (*columns)[columnIndex].SampleRange(rangeStart,
                                rangeEnd,
                                offset,
                                resolution,
                                rangeOutput);
                            offset += count;

                            if (count > 0) {
                                sp_event_t eventOut = {"/csv/column_range", ent, {SP_EVENT_DATA_TYPE_BYTES}};
                                static_assert(sizeof(ColumnRange) <= sizeof(event_bytes_t));
                                std::copy_n((const uint8_t *)&rangeOutput,
                                    sizeof(ColumnRange),
                                    &eventOut.data.bytes[0]);
                                sp_event_send(lock, event->source, &eventOut);
                            }
                            if (count < rangeOutput.size()) break;
                        }
                        sp_event_t eventOut = {"/csv/column_range", ent, {SP_EVENT_DATA_TYPE_BOOL}};
                        eventOut.data.b = true;
                        sp_event_send(lock, event->source, &eventOut);
                    } else {
                        sp_event_t eventOut = {"/csv/column_range", ent, {SP_EVENT_DATA_TYPE_BOOL}};
                        eventOut.data.b = false;
                        sp_event_send(lock, event->source, &eventOut);
                    }
                }
            } else {
                Logf("Received unknown csv event: %s = %s", event->name, event->data.type);
            }
        }
    }

    std::shared_ptr<std::vector<ColumnData>> LoadCSVData(std::string_view dataStr) {
        loadingProgress = 0;
        std::vector<ColumnData> columns;
        std::vector<std::string_view> lines;
        SplitStr(dataStr, '\n', lines);
        std::vector<std::string_view> columnNames;
        SplitStr(lines.front(), ',', columnNames);
        for (auto &col : columnNames) {
            auto &colData = columns.emplace_back(col, lines.size() - 1);
            colData.columnIndex = columns.size() - 1;
            sp_signal_ref_t colRef = {};
            sp_signal_ref_new(&entityRef, colData.name.c_str(), &colRef);
            outputs.push_back(colRef);
        }
        std::vector<std::string_view> values;
        size_t firstTimestamp = ~0llu;
        size_t lastTimestamp = 0;
        for (size_t i = 1; i < lines.size(); i++) {
            SplitStr(lines[i], ',', values);
            double number = ParseNumber(values.front());
            size_t intervalCol = (size_t)number;
            if (std::isfinite(number) && number >= 0) {
                firstTimestamp = std::min(firstTimestamp, intervalCol);
                lastTimestamp = std::max(lastTimestamp, intervalCol);
            }
            for (size_t c = 0; c < values.size(); c++) {
                double num = ParseNumber(values[c]);
                if (!std::isnan(num)) {
                    if (num < columns[c].min) columns[c].min = num;
                    if (num > columns[c].max) columns[c].max = num;
                    columns[c].data.emplace_back(intervalCol, num);
                }
            }
            loadingProgress = i * 100 / lines.size();
        }
        for (size_t c = 0; c < columns.size(); c++) {
            columns[c].firstTimestamp = firstTimestamp;
            columns[c].lastTimestamp = lastTimestamp;
        }
        loadingProgress = 100;

        {
            auto *lock = Tecs_ecs_start_transaction(sp_get_live_ecs(), 1 | SP_ACCESS_SIGNALS, SP_ACCESS_SIGNALS);
            sp_signal_ref_clear_value(&loadingRef, lock);
            auto &utcData = columns.at(1).data;
            for (auto &[sampleTimeMs, value] : utcData) {
                if (value > 0.0) {
                    currentTimeNs = sampleTimeMs * 1e6;
                    break;
                }
            }
            Tecs_lock_release(lock);
        }
        return std::make_shared<std::vector<ColumnData>>(std::move(columns));
    }

    void OnTick(sp_script_state_t *state, tecs_lock_t *lock, tecs_entity_t ent, uint64_t intervalNs) {
        if (!sp_entity_ref_is_valid(&entityRef)) {
            sp_entity_ref_new(ent, &entityRef);
            sp_signal_ref_new(&entityRef, "loading", &loadingRef);
            sp_signal_ref_new(&entityRef, "accel_x", &accelXRef);
            sp_signal_ref_new(&entityRef, "accel_y", &accelYRef);
            sp_signal_ref_new(&entityRef, "accel_z", &accelZRef);
            sp_signal_ref_new(&entityRef, "x", &xRef);
            sp_signal_ref_new(&entityRef, "y", &yRef);
            sp_signal_ref_new(&entityRef, "z", &zRef);
        }

        if (!columnsPtr || filename != loaded) {
            for (auto &ref : outputs) {
                sp_signal_ref_clear_value(&ref, lock);
            }
            outputs.clear();
            sp_signal_ref_set_value(&loadingRef, lock, 1.0);

            assetPtr = workQueue->Dispatch<std::vector<char>>([this] {
                std::ifstream stream(filename.c_str(), std::ios::in | std::ios::binary);

                if (stream) {
                    stream.seekg(0, std::ios::end);
                    auto size = stream.tellg();
                    stream.seekg(0, std::ios::beg);

                    std::shared_ptr<std::vector<char>> buffer = std::make_shared<std::vector<char>>(size);
                    stream.read((char *)buffer->data(), size);
                    Assertf(stream.good(), "Failed to read whole asset file: %s", filename);
                    stream.close();
                    return buffer;
                }
                return std::shared_ptr<std::vector<char>>();
            });
            columnsPtr = workQueue->Dispatch<std::vector<ColumnData>>(assetPtr,
                [this](std::shared_ptr<std::vector<char>> asset) {
                    if (!asset) return std::shared_ptr<std::vector<ColumnData>>();
                    return LoadCSVData(std::string_view(asset->data(), asset->size()));
                });
            loaded = filename;
        }
        HandleEvents(state, lock, ent);
        if (!columnsPtr->Ready()) return;
        auto columns = columnsPtr->Get();
        if (!columns) return;
        for (size_t i = 0; i < columns->size(); i++) {
            double value = (*columns)[i].SampleTimestamp(currentTimeNs / 1e6);
            if (std::isnan(value)) {
                sp_signal_ref_clear_value(&outputs[i], lock);
            } else {
                sp_signal_ref_set_value(&outputs[i], lock, value);
            }
        }
        if (Tecs_entity_has_transform_tree(lock, ent)) {
            vec3_t signalPos = {
                (float)sp_signal_ref_get_signal(&xRef, lock, 0),
                (float)sp_signal_ref_get_signal(&yRef, lock, 0),
                (float)sp_signal_ref_get_signal(&zRef, lock, 0),
            };
            sp_ecs_transform_tree_t *transformTree = Tecs_entity_get_transform_tree(lock, ent);
            sp_transform_set_position(&transformTree->transform, &signalPos);

            const glm::vec3 *currPosition = (const glm::vec3 *)sp_transform_get_position(&transformTree->transform);
            glm::vec3 direction = lastDir;
            auto deltaPos = *currPosition - lastPosition;
            if (glm::length2(deltaPos) > 0.001) {
                direction = -glm::normalize(deltaPos);
            }
            glm::quat oldDir = glm::quatLookAt(lastDir, glm::vec3(0, 1, 0));
            glm::quat newDir = glm::quatLookAt(direction, glm::vec3(0, 1, 0));
            newDir = glm::normalize(glm::slerp(oldDir, newDir, 0.2f));
            sp_transform_set_rotation(&transformTree->transform, (const quat_t *)&newDir);

            lastPosition = *(const glm::vec3 *)sp_transform_get_position(&transformTree->transform);
            lastDir = newDir * glm::vec3(0, 0, -1);

            glm::vec3 down = {
                (float)sp_signal_ref_get_signal(&accelXRef, lock, 0),
                (float)sp_signal_ref_get_signal(&accelYRef, lock, 0),
                (float)sp_signal_ref_get_signal(&accelZRef, lock, 0),
            };
            if (!glm::all(glm::epsilonEqual(down, glm::vec3(0), 0.001f))) {
                down = glm::normalize(down);
            } else {
                down = glm::vec3(0, -1, 0);
            }
            glm::quat rotation = glm::rotation(glm::vec3(0, -1, 0), down);
            sp_transform_rotate(&transformTree->transform, (const quat_t *)&rotation);
        }
        currentTimeNs += intervalNs;
    }

    static void DefaultInit(void *context) {
        CSVVisualizer *ctx = static_cast<CSVVisualizer *>(context);
        new (ctx) CSVVisualizer();
    }

    static void Init(void *context, sp_script_state_t *state) {
        CSVVisualizer *ctx = static_cast<CSVVisualizer *>(context);
        ctx->workQueue.emplace("CSVWorker");
    }

    static void Destroy(void *context, sp_script_state_t *state) {
        CSVVisualizer *ctx = static_cast<CSVVisualizer *>(context);
        ctx->~CSVVisualizer();
    }

    static void OnTickLogic(void *context,
        sp_script_state_t *state,
        tecs_lock_t *lock,
        tecs_entity_t ent,
        uint64_t intervalNs) {
        CSVVisualizer *ctx = static_cast<CSVVisualizer *>(context);
        ctx->OnTick(state, lock, ent, intervalNs);
    }
};

extern "C" {

PLUGIN_EXPORT size_t sp_plugin_get_script_definitions(sp_dynamic_script_definition_t *output, size_t output_size) {
    if (output_size >= 1 && output != NULL) {
        std::strncpy(output[0].name, "csv_visualizer2", sizeof(output[0].name) - 1);
        output[0].type = SP_SCRIPT_TYPE_LOGIC_SCRIPT;
        output[0].context_size = sizeof(CSVVisualizer);
        output[0].default_init_func = &CSVVisualizer::DefaultInit;
        output[0].init_func = &CSVVisualizer::Init;
        output[0].destroy_func = &CSVVisualizer::Destroy;
        output[0].on_tick_func = &CSVVisualizer::OnTickLogic;
        output[0].filter_on_event = false;
        event_name_t *events = sp_event_name_vector_resize(&output[0].events, 2);
        std::strncpy(events[0], "/csv/get_metadata", sizeof(events[0]) - 1);
        std::strncpy(events[1], "/csv/query_range", sizeof(events[1]) - 1);

        sp_struct_field_t *fields = sp_struct_field_vector_resize(&output[0].fields, 2);
        sp_string_set(&fields[0].name, "filename");
        fields[0].type.type_index = SP_TYPE_INDEX_EVENT_STRING;
        fields[0].size = sizeof(CSVVisualizer::filename);
        fields[0].offset = offsetof(CSVVisualizer, filename);
        sp_string_set(&fields[1].name, "current_time_ns");
        fields[1].type.type_index = SP_TYPE_INDEX_UINT64;
        fields[1].size = sizeof(CSVVisualizer::currentTimeNs);
        fields[1].offset = offsetof(CSVVisualizer, currentTimeNs);
    }
    return 1;
}

} // extern "C"
