/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "gui_script.h"
#include "shared_events.h"

#include <algorithm>
#include <c_abi/Tecs.hh>
#include <c_abi/strayphotons_ecs_c_abi_entity_gen.h>
#include <c_abi/strayphotons_ecs_c_abi_lock_gen.h>
#include <common/InlineString.hh>
#include <common/Logging.hh>
#include <cstdint>
#include <glm/glm.hpp>
#include <gui/ImGuiHelpers.hh>
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <implot_internal.h>
#include <sstream>
#include <strayphotons/components.h>
#include <vector>

struct HeatmapData {
    std::vector<SampleAverage> data;
    glm::uvec2 extents = {32, 32};
    glm::ivec3 axisIndex = {12, 21, 14};
    uint64_t rangeStart, rangeSize;
};

struct HeatmapDisplayGui {
    sp_entity_ref_t guiDefinitionEntityRef = {};
    sp_entity_ref_t dataEntityRef = {};

    uint64_t viewStart = 0, viewSize = 0;
    int loadingProgress = 0;
    bool columnsLoaded = false;
    bool reloadingHeatmap = false;
    bool awaitingResponse = false;
    HeatmapData heatmap = {};
    std::vector<ColumnMetadata> columns;

    bool hasFocus;

    bool BeforeFrame(sp_script_state_t *state, tecs_entity_t ent) {
        tecs_lock_t *lock = Tecs_ecs_start_transaction(sp_get_live_ecs(), 1 | SP_ACCESS_FOCUS_LOCK, 0);
        const sp_ecs_focus_lock_t *focusLock = sp_ecs_get_const_focus_lock(lock);
        hasFocus = sp_ecs_focus_lock_has_focus(focusLock, SP_FOCUS_LAYER_HUD);
        Tecs_lock_release(lock);

        return true;
    }

    void PreDefine(sp_script_state_t *state, tecs_entity_t ent) {
        if (heatmap.rangeStart != viewStart || heatmap.rangeSize != viewSize) {
            heatmap.rangeStart = viewStart;
            heatmap.rangeSize = viewSize;
            heatmap.data.clear();
            heatmap.data.resize(heatmap.extents.x * heatmap.extents.y, {});
            reloadingHeatmap = true;
        }

        ImPlot::PushColormap(ImPlotColormap_Jet);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, hasFocus ? 0.8f : 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        PushFont(sp::GuiFont::Primary, 16);
    }

    void DefineContents(sp_compositor_ctx_t *compositor, sp_script_state_t *state, tecs_entity_t ent) {
        // auto &io = ImGui::GetIO();

        // auto lock = StartTransaction<ReadSignalsLock>();
        tecs_ecs_t *liveEcs = sp_get_live_ecs();
        tecs_lock_t *lock = Tecs_ecs_start_transaction(liveEcs,
            1 | SP_ACCESS_NAME | SP_ACCESS_SIGNALS | SP_ACCESS_SIGNAL_OUTPUT | SP_ACCESS_SIGNAL_BINDINGS |
                SP_ACCESS_EVENT_INPUT | SP_ACCESS_EVENT_BINDINGS | SP_ACCESS_FOCUS_LOCK,
            0);

        sp_event_t *event;
        while ((event = sp_script_state_poll_event(state, lock))) {
            if (strcmp(event->name, "/csv/loading_progress") == 0) {
                awaitingResponse = false;
                if (event->data.type == SP_EVENT_DATA_TYPE_INT) {
                    loadingProgress = event->data.i;
                }
            } else if (strcmp(event->name, "/csv/column_metadata") == 0) {
                awaitingResponse = false;
                if (event->data.type == SP_EVENT_DATA_TYPE_BOOL) {
                    if (event->data.b) {
                        columnsLoaded = true;
                        heatmap.data.clear();
                        heatmap.data.resize(heatmap.extents.x * heatmap.extents.y, {});
                        reloadingHeatmap = true;
                    }
                } else if (event->data.type == SP_EVENT_DATA_TYPE_BYTES) {
                    const ColumnMetadata *metadata = (const ColumnMetadata *)&event->data.bytes[0];
                    if (metadata->columnIndex == columns.size()) {
                        columns.push_back(*metadata);
                    } else if (metadata->columnIndex < columns.size()) {
                        columns[metadata->columnIndex] = *metadata; // Overwrites existing data
                    } else {
                        Warnf("Invalid column metadata index: %s %u", event->name, metadata->columnIndex);
                    }
                } else {
                    Warnf("Invalid event type: %s %s", event->name, event->data.type);
                }
            } else if (strcmp(event->name, "/csv/heatmap_data") == 0) {
                if (event->data.type == SP_EVENT_DATA_TYPE_BOOL) {
                    Logf("Heatmap loading complete");
                    awaitingResponse = false;
                    reloadingHeatmap = false;
                } else if (event->data.type == SP_EVENT_DATA_TYPE_BYTES) {
                    const HeatmapSamples *samples = (const HeatmapSamples *)&event->data.bytes[0];
                    if (samples->indexOffset < heatmap.data.size()) {
                        std::copy_n(samples->begin(),
                            std::min((size_t)samples->sampleCount, heatmap.data.size() - samples->indexOffset),
                            heatmap.data.data() + samples->indexOffset);
                    }
                } else {
                    Warnf("Invalid event type: %s %s", event->name, event->data.type);
                }
            } else if (strcmp(event->name, "/gui/imgui_input") == 0) {
                if (event->data.type == SP_EVENT_DATA_TYPE_BYTES) {
                    ImGuiInputEvent inputEvent = reinterpret_cast<const ImGuiInputEvent &>(event->data.bytes);
                    ImGuiContext &imCtx = *ImGui::GetCurrentContext();
                    inputEvent.EventId = imCtx.InputEventsNextEventId++;
                    imCtx.InputEventsQueue.push_back(inputEvent);
                } else {
                    Warnf("Invalid event type: %s %s", event->name, event->data.type);
                }
            } else {
                Logf("Received graph event: %s = %s", event->name, event->data.type);
            }
        }

        if (!columnsLoaded && !awaitingResponse) {
            sp_entity_t guiDefinitionEnt = sp_entity_ref_get(&guiDefinitionEntityRef, lock);
            sp_event_t eventOut = {"/csv/get_metadata", guiDefinitionEnt, {SP_EVENT_DATA_TYPE_UINT}};
            eventOut.data.ui = columns.size();
            sp_event_send_ref(lock, &dataEntityRef, &eventOut);
            awaitingResponse = true;
        }
        if (hasFocus) {
            ImVec4 textColor(1, 1, 1, 1);
            ImGui::PushStyleColor(ImGuiCol_Text, textColor);
            ImGui::PushStyleColor(ImGuiCol_Border, textColor);
            ImGui::BeginChild("csv_heatmap", ImVec2(-FLT_MIN, -FLT_MIN), true);

            if (!columnsLoaded) {
                ImGui::Text("Loading... (%d%%)", loadingProgress);
            } else {
                ImGui::PushItemWidth(150.0f);
                if (ImGui::Combo(
                        "X Axis##x_axis",
                        &heatmap.axisIndex.x,
                        [](void *data, int i) {
                            auto *columns = (std::vector<ColumnMetadata> *)data;
                            return columns->at(i).name.c_str();
                        },
                        &columns,
                        columns.size())) {
                    heatmap.data.clear();
                    heatmap.data.resize(heatmap.extents.x * heatmap.extents.y);
                    reloadingHeatmap = true;
                }
                ImGui::SameLine();
                if (ImGui::Combo(
                        "Y Axis##y_axis",
                        &heatmap.axisIndex.y,
                        [](void *data, int i) {
                            auto *columns = (std::vector<ColumnMetadata> *)data;
                            return columns->at(i).name.c_str();
                        },
                        &columns,
                        columns.size())) {
                    heatmap.data.clear();
                    heatmap.data.resize(heatmap.extents.x * heatmap.extents.y);
                    reloadingHeatmap = true;
                }
                ImGui::SameLine();
                if (ImGui::Combo(
                        "Z Axis##z_axis",
                        &heatmap.axisIndex.z,
                        [](void *data, int i) {
                            auto *columns = (std::vector<ColumnMetadata> *)data;
                            return columns->at(i).name.c_str();
                        },
                        &columns,
                        columns.size())) {
                    heatmap.data.clear();
                    heatmap.data.resize(heatmap.extents.x * heatmap.extents.y);
                    reloadingHeatmap = true;
                }
                ImGui::PopItemWidth();

                // auto heatmapRegion = ImGui::GetContentRegionAvail();
                // auto startCursor = ImGui::GetCursorScreenPos();
                // for (size_t y = 0; y < heatmap.extents.y; y++) {
                //     for (size_t x = 0; x < heatmap.extents.x; x++) {
                //         ImGui::SetCursorScreenPos(ImVec2(startCursor.x + x * heatmapRegion.x / heatmap.extents.x,
                //             startCursor.y + heatmapRegion.y - y * heatmapRegion.y / heatmap.extents.y));
                //         // if (x > 0) ImGui::SameLine();
                //         auto index = x + y * heatmap.extents.x;
                //         if (index < heatmap.data.size()) {
                //             auto &sample = heatmap.data[index];
                //             if (sample.sampleCount > 0) {
                //                 // ImGui::Text("%u", sample.sampleCount);
                //                 ImGui::Text("%.1f", sample.sum / sample.sampleCount);
                //             } else {
                //                 ImGui::TextUnformatted("0");
                //             }
                //         } else {
                //             ImGui::TextUnformatted("err");
                //         }
                //     }
                // }

                if (glm::all(glm::greaterThanEqual(heatmap.axisIndex, glm::ivec3(0))) &&
                    glm::all(glm::lessThan(heatmap.axisIndex, glm::ivec3(columns.size())))) {
                    auto &xColumn = columns[heatmap.axisIndex.x];
                    auto &yColumn = columns[heatmap.axisIndex.y];
                    auto &zColumn = columns[heatmap.axisIndex.z];

                    static std::vector<float> values;
                    values.clear();
                    values.resize(heatmap.extents.x * heatmap.extents.y);
                    for (size_t y = 0; y < heatmap.extents.y; y++) {
                        for (size_t x = 0; x < heatmap.extents.x; x++) {
                            size_t i = x + y * heatmap.extents.x;
                            size_t j = x + (heatmap.extents.y - y - 1) * heatmap.extents.x;
                            if (heatmap.data[i].sampleCount <= 0) continue;
                            values[j] = heatmap.data[i].sum / heatmap.data[i].sampleCount;
                        }
                    }

                    if (ImPlot::BeginPlot("##Heatmap1",
                            ImGui::GetContentRegionAvail(),
                            ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
                        ImPlot::SetupAxes(xColumn.name.c_str(), yColumn.name.c_str());
                        ImPlot::SetupAxisTicks(ImAxis_X1, xColumn.min, xColumn.max, heatmap.extents.x + 1);
                        ImPlot::SetupAxisTicks(ImAxis_Y1, yColumn.min, yColumn.max, heatmap.extents.y + 1);
                        ImPlot::PlotHeatmap("heat",
                            values.data(),
                            heatmap.extents.x,
                            heatmap.extents.y,
                            zColumn.min,
                            zColumn.max,
                            "%.1f",
                            ImPlotPoint(xColumn.min, yColumn.min),
                            ImPlotPoint(xColumn.max, yColumn.max)); // ImPlotHeatmapFlags_ColMajor
                        ImPlot::EndPlot();
                    }
                    ImGui::SameLine();
                    ImPlot::ColormapScale("##HeatScale", zColumn.min, zColumn.max, ImVec2(60, 225));
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
        }

        if (columnsLoaded && !awaitingResponse && reloadingHeatmap) {
            sp_entity_t guiDefinitionEnt = sp_entity_ref_get(&guiDefinitionEntityRef, lock);
            sp_event_t eventOut2 = {"/csv/query_heatmap", guiDefinitionEnt, {SP_EVENT_DATA_TYPE_BYTES}};
            (*reinterpret_cast<HeatmapQuery *>(&eventOut2.data.bytes[0])) = HeatmapQuery{
                heatmap.rangeStart,
                heatmap.rangeSize,
                heatmap.extents,
                heatmap.axisIndex,
            };
            sp_event_send_ref(lock, &dataEntityRef, &eventOut2);
            awaitingResponse = true;
        }

        Tecs_lock_release(lock);
    }

    void PostDefine(sp_script_state_t *state, tecs_entity_t ent) {
        ImGui::PopFont();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        ImPlot::PopColormap();
    }
};

extern "C" {

PLUGIN_EXPORT size_t sp_plugin_get_script_definitions(sp_dynamic_script_definition_t *output, size_t output_size) {
    if (output_size >= 1 && output != NULL) {
        std::strncpy(output[0].name, "csv_heatmap", sizeof(output[0].name) - 1);
        output[0].type = SP_SCRIPT_TYPE_GUI_SCRIPT;
        output[0].context_size = sizeof(ScriptGuiContext<HeatmapDisplayGui>);
        output[0].default_init_func = &ScriptGuiContext<HeatmapDisplayGui>::DefaultInit;
        output[0].init_func = &ScriptGuiContext<HeatmapDisplayGui>::Init;
        output[0].destroy_func = &ScriptGuiContext<HeatmapDisplayGui>::Destroy;
        output[0].before_frame_func = &ScriptGuiContext<HeatmapDisplayGui>::BeforeFrameStatic;
        output[0].render_gui_func = &ScriptGuiContext<HeatmapDisplayGui>::RenderGui;
        output[0].filter_on_event = false;
        event_name_t *events = sp_event_name_vector_resize(&output[0].events, 3);
        std::strncpy(events[0], "/csv/column_metadata", sizeof(events[0]) - 1);
        std::strncpy(events[1], "/csv/heatmap_data", sizeof(events[1]) - 1);
        std::strncpy(events[2], "/csv/loading_progress", sizeof(events[2]) - 1);

        sp_struct_field_t *fields = sp_struct_field_vector_resize(&output[0].fields, 4);
        sp_string_set(&fields[0].name, "range_start");
        fields[0].type.type_index = SP_TYPE_INDEX_UINT64;
        fields[0].size = sizeof(HeatmapDisplayGui::viewStart);
        fields[0].offset = offsetof(ScriptGuiContext<HeatmapDisplayGui>, gui.viewStart);
        sp_string_set(&fields[1].name, "range_size");
        fields[1].type.type_index = SP_TYPE_INDEX_UINT64;
        fields[1].size = sizeof(HeatmapDisplayGui::viewSize);
        fields[1].offset = offsetof(ScriptGuiContext<HeatmapDisplayGui>, gui.viewSize);
        sp_string_set(&fields[2].name, "gui_definition_entity");
        fields[2].type.type_index = SP_TYPE_INDEX_ENTITY_REF;
        fields[2].size = sizeof(HeatmapDisplayGui::guiDefinitionEntityRef);
        fields[2].offset = offsetof(ScriptGuiContext<HeatmapDisplayGui>, gui.guiDefinitionEntityRef);
        sp_string_set(&fields[3].name, "data_entity");
        fields[3].type.type_index = SP_TYPE_INDEX_ENTITY_REF;
        fields[3].size = sizeof(HeatmapDisplayGui::dataEntityRef);
        fields[3].offset = offsetof(ScriptGuiContext<HeatmapDisplayGui>, gui.dataEntityRef);
    }
    return 1;
}

} // extern "C"
