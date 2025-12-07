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
#include <common/Logging.hh>
#include <fstream>
#include <glm/glm.hpp>
#include <gui/ImGuiHelpers.hh>
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <implot_internal.h>
#include <strayphotons/components.h>
#include <vector>

namespace sp {
    enum class GuiFont {
        Primary,
        Accent,
        Monospace,
    };

    struct GuiFontDef {
        GuiFont type;
        const char *name;
        float size;
    };
} // namespace sp

static const ImWchar glyphRanges[] = {
    0x0020,
    0x00FF, // Basic Latin + Latin Supplement
    0x2100,
    0x214F, // Letterlike Symbols
    0,
};

static const std::array fontList = {
    sp::GuiFontDef{sp::GuiFont::Primary, "DroidSans-Regular.ttf", 16.0f},
    sp::GuiFontDef{sp::GuiFont::Primary, "DroidSans-Regular.ttf", 32.0f},
    sp::GuiFontDef{sp::GuiFont::Monospace, "3270SemiCondensed-Regular.ttf", 25.0f},
    sp::GuiFontDef{sp::GuiFont::Monospace, "3270SemiCondensed-Regular.ttf", 32.0f},
};

void PushFont(sp::GuiFont fontType, float fontSize) {
    auto &io = ImGui::GetIO();
    // Assert(io.Fonts->Fonts.size() == fontList.size() + 1, "unexpected font list size");

    for (size_t i = 0; i < fontList.size(); i++) {
        auto &f = fontList[i];
        if (f.type == fontType && f.size == fontSize) {
            ImGui::PushFont(io.Fonts->Fonts[i + 1]);
            return;
        }
    }

    // Abortf("missing font type %d with size %f", (int)fontType, fontSize);
}

template<typename GuiDef>
class ScriptGuiContext : public GuiDef {
public:
    ScriptGuiContext() : GuiDef() {}

    static void DefaultInit(void *context) {
        ScriptGuiContext<GuiDef> *ctx = static_cast<ScriptGuiContext<GuiDef> *>(context);
        new (ctx) ScriptGuiContext<GuiDef>();
    }

    static void Init(void *context, sp_script_state_t *state) {
        ScriptGuiContext<GuiDef> *ctx = static_cast<ScriptGuiContext<GuiDef> *>(context);
        size_t eventIndex = sp_event_name_vector_get_size(&state->definition.events);
        sp_event_name_vector_resize(&state->definition.events, eventIndex + 1);
        event_name_t *events = sp_event_name_vector_get_data(&state->definition.events);
        std::strncpy(events[eventIndex], "/gui/imgui_input", sizeof(events[eventIndex]) - 1);

        ctx->imCtx = ImGui::CreateContext();
        ctx->imPlot = ImPlot::CreateContext();
        ctx->fontAtlas = make_shared<ImFontAtlas>();
        ctx->fontAtlas->AddFontDefault(nullptr);

        for (auto &def : fontList) {
            // auto asset = Assets().Load("fonts/"s + def.name)->Get();
            // Assertf(asset, "Failed to load gui font %s", def.name);
            auto filename = "../assets/fonts/"s + def.name;
            std::ifstream stream(filename.c_str(), std::ios::in | std::ios::binary);

            if (stream) {
                stream.seekg(0, std::ios::end);
                auto size = stream.tellg();
                stream.seekg(0, std::ios::beg);

                std::vector<uint8_t> buffer(size);
                stream.read((char *)buffer.data(), size);
                Assertf(stream.good(), "Failed to read whole asset file: %s", filename);
                stream.close();

                ImFontConfig cfg = {};
                cfg.FontData = (void *)buffer.data();
                cfg.FontDataSize = buffer.size();
                cfg.FontDataOwnedByAtlas = false;
                cfg.SizePixels = def.size;
                cfg.GlyphRanges = &glyphRanges[0];
                strncpy(cfg.Name, filename.c_str(), std::min(sizeof(cfg.Name) - 1, filename.length()));
                ctx->fontAtlas->AddFont(&cfg);
            }
        }

        uint8 *fontData;
        int fontWidth, fontHeight;
        ctx->fontAtlas->GetTexDataAsRGBA32(&fontData, &fontWidth, &fontHeight);

        ImGuiIO &io = ImGui::GetIO(ctx->imCtx);
        ctx->originalAtlas = io.Fonts;
        io.Fonts = ctx->fontAtlas.get();
        io.Fonts->TexID = 1ull << 16; // FONT_ATLAS_ID
        io.IniFilename = nullptr;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    }

    static void Destroy(void *context, sp_script_state_t *state) {
        ScriptGuiContext<GuiDef> *ctx = static_cast<ScriptGuiContext<GuiDef> *>(context);
        ImGui::GetIO(ctx->imCtx).Fonts = ctx->originalAtlas;
        ImPlot::DestroyContext(ctx->imPlot);
        ImGui::DestroyContext(ctx->imCtx);
        ctx->imCtx = nullptr;
    }

    static bool BeforeFrameStatic(void *context, sp_script_state_t *state, tecs_entity_t ent) {
        ScriptGuiContext<GuiDef> *ctx = static_cast<ScriptGuiContext<GuiDef> *>(context);
        if (!ctx->imCtx) return false;

        return ctx->BeforeFrame(state, ent);
    }

    static void RenderGui(void *context,
        sp_script_state_t *state,
        tecs_entity_t ent,
        vec2_t displaySize,
        vec2_t scale,
        float deltaTime,
        sp_gui_draw_data_t *result) {
        ScriptGuiContext<GuiDef> *ctx = static_cast<ScriptGuiContext<GuiDef> *>(context);
        if (!ctx->imCtx) return;
        ImGui::SetCurrentContext(ctx->imCtx);
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2(displaySize.v[0], displaySize.v[1]);
        io.DisplayFramebufferScale = ImVec2(scale.v[0], scale.v[1]);
        io.DeltaTime = deltaTime;

        ImGui::NewFrame();

        // DefineWindows
        int flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;

        ctx->PreDefine(state, ent);
        // static bool open = true;
        // ImPlot::ShowDemoWindow(&open);
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
        ImGui::Begin("signal_display", nullptr, flags);
        ctx->DefineContents(state, ent);
        ImGui::End();
        ctx->PostDefine(state, ent);

        ImGui::Render();

        auto *drawData = ImGui::GetDrawData();
        drawData->ScaleClipRects(io.DisplayFramebufferScale);
        sp::ConvertImDrawData(drawData, result);
    }

private:
    ImGuiContext *imCtx = nullptr;
    ImPlotContext *imPlot = nullptr;
    shared_ptr<ImFontAtlas> fontAtlas;
    ImFontAtlas *originalAtlas = nullptr;
};

struct SignalDisplayGui {
    std::string prefix = "";
    std::string suffix = "mW";
    std::streamsize precision = 2;

    bool BeforeFrame(sp_script_state_t *state, tecs_entity_t ent) {
        return true;
    }

    void PreDefine(sp_script_state_t *state, tecs_entity_t ent) {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        PushFont(sp::GuiFont::Monospace, 32);
    }

    void DefineContents(sp_script_state_t *state, tecs_entity_t ent) {
        // auto lock = StartTransaction<ReadSignalsLock>();
        tecs_ecs_t *liveEcs = sp_get_live_ecs();
        tecs_lock_t *lock = Tecs_ecs_start_transaction(liveEcs,
            1 | SP_ACCESS_NAME | SP_ACCESS_SIGNALS | SP_ACCESS_SIGNAL_OUTPUT | SP_ACCESS_SIGNAL_BINDINGS |
                SP_ACCESS_EVENT_INPUT | SP_ACCESS_FOCUS_LOCK,
            0);

        std::string text = "error";
        ImVec4 textColor(1, 0, 0, 1);
        if (Tecs_entity_exists(lock, ent)) {
            sp_entity_ref_t entRef = {};
            sp_entity_ref_new(ent, &entRef);

            sp_signal_ref_t maxValueRef = {};
            sp_signal_ref_new(&entRef, "max_value", &maxValueRef);
            double maxValue = sp_signal_ref_get_signal(&maxValueRef, lock, 0);

            sp_signal_ref_t valueRef = {};
            sp_signal_ref_new(&entRef, "value", &valueRef);
            double value = sp_signal_ref_get_signal(&valueRef, lock, 0);

            sp_signal_ref_t textColorRRef = {};
            sp_signal_ref_new(&entRef, "text_color_r", &textColorRRef);
            textColor.x = sp_signal_ref_get_signal(&textColorRRef, lock, 0);

            sp_signal_ref_t textColorGRef = {};
            sp_signal_ref_new(&entRef, "text_color_g", &textColorGRef);
            textColor.y = sp_signal_ref_get_signal(&textColorGRef, lock, 0);

            sp_signal_ref_t textColorBRef = {};
            sp_signal_ref_new(&entRef, "text_color_b", &textColorBRef);
            textColor.z = sp_signal_ref_get_signal(&textColorBRef, lock, 0);

            std::stringstream ss;
            if (maxValue != 0.0) {
                ss << prefix << std::fixed << std::setprecision(precision) << (value / maxValue * 100.0) << "%";
            } else {
                ss << prefix << std::fixed << std::setprecision(precision) << value << suffix;
            }
            text = ss.str();
        }

        Tecs_lock_release(lock);

        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::PushStyleColor(ImGuiCol_Border, textColor);
        ImGui::BeginChild("signal_display", ImVec2(-FLT_MIN, -FLT_MIN), true);

        auto textSize = ImGui::CalcTextSize(text.c_str());
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textSize.x) * 0.5f);
        ImGui::SetCursorPosY((ImGui::GetWindowSize().y - textSize.y) * 0.5f);
        ImGui::TextUnformatted(text.c_str());

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
    }

    void PostDefine(sp_script_state_t *state, tecs_entity_t ent) {
        ImGui::PopFont();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }
};

struct ColumnData : public ColumnMetadata {
    std::vector<MinMaxSample> data;

    ColumnData(const ColumnMetadata &metadata) : ColumnMetadata(metadata) {}
};

struct GraphDisplayGui {
    sp_entity_ref_t guiDefinitionEntityRef = {};
    sp_entity_ref_t dataEntityRef = {};
    int32_t selectedColumn = 0;
    uint64_t rangeStart, rangeSize, rangeResolution;

    uint64_t viewStart = 0, viewSize = 0;
    size_t viewResolution = 1000;
    int loadingProgress = 0;
    bool columnsLoaded = false;
    bool reloadingColumns = false;
    int32_t reloadingColumn = 0;
    bool awaitingResponse = false;
    std::vector<ColumnData> columns;

    bool draggingView = false;
    ImVec2 dragStartPos;

    bool BeforeFrame(sp_script_state_t *state, tecs_entity_t ent) {
        tecs_lock_t *lock = Tecs_ecs_start_transaction(sp_get_live_ecs(), 1 | SP_ACCESS_FOCUS_LOCK, 0);
        const sp_ecs_focus_lock_t *focusLock = sp_ecs_get_const_focus_lock(lock);
        bool hasFocus = sp_ecs_focus_lock_has_focus(focusLock, SP_FOCUS_LAYER_HUD);
        Tecs_lock_release(lock);

        return hasFocus;
    }

    void PreDefine(sp_script_state_t *state, tecs_entity_t ent) {
        if (rangeStart != viewStart || rangeSize != viewSize) {
            if (reloadingColumn < 0) reloadingColumn = selectedColumn;
            rangeStart = viewStart;
            rangeSize = viewSize;
        }
        if (rangeResolution != viewResolution) {
            for (auto &column : columns) {
                column.data.clear();
                column.data.resize(viewResolution, {NAN, NAN});
            }
            rangeStart = viewStart;
            rangeSize = viewSize;
            rangeResolution = viewResolution;
            reloadingColumn = 0;
            reloadingColumns = true;
        }

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        PushFont(sp::GuiFont::Primary, 16);
    }

    void DefineContents(sp_script_state_t *state, tecs_entity_t ent) {
        ImVec4 textColor(1, 1, 1, 1);
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::PushStyleColor(ImGuiCol_Border, textColor);
        ImGui::BeginChild("signal_graph", ImVec2(-FLT_MIN, -FLT_MIN), true);

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
                        reloadingColumn = 0;
                        reloadingColumns = true;
                    }
                } else if (event->data.type == SP_EVENT_DATA_TYPE_BYTES) {
                    const ColumnMetadata *metadata = (const ColumnMetadata *)&event->data.bytes[0];
                    if (metadata->columnIndex == columns.size()) {
                        columns.push_back(*metadata);
                        columns.back().data.resize(rangeResolution);
                    } else if (metadata->columnIndex < columns.size()) {
                        columns[metadata->columnIndex] = *metadata; // Clears existing data
                        columns[metadata->columnIndex].data.resize(rangeResolution);
                    } else {
                        Warnf("Invalid column metadata index: %s %u", event->name, metadata->columnIndex);
                    }
                } else {
                    Warnf("Invalid event type: %s %s", event->name, event->data.type);
                }
            } else if (strcmp(event->name, "/csv/column_range") == 0) {
                if (event->data.type == SP_EVENT_DATA_TYPE_BOOL) {
                    awaitingResponse = false;
                    if (reloadingColumns) {
                        if (reloadingColumn >= columns.size()) {
                            reloadingColumn = -1;
                            reloadingColumns = false;
                        } else {
                            reloadingColumn++;
                        }
                    } else if (reloadingColumn >= 0) {
                        reloadingColumn = -1;
                    }
                } else if (event->data.type == SP_EVENT_DATA_TYPE_BYTES) {
                    const ColumnRange *colRange = (const ColumnRange *)&event->data.bytes[0];
                    if (colRange->columnIndex >= 0 && colRange->columnIndex < columns.size()) {
                        auto &colData = columns[colRange->columnIndex].data;
                        if (colRange->sampleOffset < colData.size()) {
                            std::copy_n(colRange->begin(),
                                std::min((size_t)colRange->sampleCount, colData.size() - colRange->sampleOffset),
                                colData.data() + colRange->sampleOffset);
                        }
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

        if (!columnsLoaded) {
            if (!awaitingResponse) {
                sp_entity_t guiDefinitionEnt = sp_entity_ref_get(&guiDefinitionEntityRef, lock);
                sp_event_t eventOut = {"/csv/get_metadata", guiDefinitionEnt, {SP_EVENT_DATA_TYPE_UINT}};
                eventOut.data.ui = columns.size();
                sp_event_send_ref(lock, &dataEntityRef, &eventOut);
                awaitingResponse = true;
            }

            ImGui::Text("Loading... (%d%%)", loadingProgress);
        } else {
            if (ImGui::Button("Reset View") || viewSize == 0) {
                size_t minTimestamp = ~0llu;
                size_t maxTimestamp = 0;
                for (size_t columnIndex = 0; columnIndex < columns.size(); columnIndex++) {
                    auto &column = columns[columnIndex];
                    minTimestamp = std::min(minTimestamp, column.firstTimestamp);
                    maxTimestamp = std::max(maxTimestamp, column.lastTimestamp);
                }
                viewStart = minTimestamp;
                viewSize = maxTimestamp - minTimestamp;
                reloadingColumn = 0;
                reloadingColumns = true;
            }
            ImVec2 plotOffset = ImGui::GetCursorScreenPos();
            ImGui::BeginChild("ScrollRegion", ImVec2(-1, -1));
            viewResolution = 0;
            for (size_t columnIndex = 0; columnIndex < columns.size(); columnIndex++) {
                auto &column = columns[columnIndex];
                if (ImGui::CollapsingHeader((column.name + "##graphHeader").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::SetNextItemWidth(-1);
                    ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 0);
                    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));
                    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.0, 0.1));
                    ImPlot::PushStyleVar(ImPlotStyleVar_LegendPadding, ImVec2(3.0, 0.0));
                    ImPlot::PushStyleVar(ImPlotStyleVar_LegendInnerPadding, ImVec2(3.0, 0.0));
                    if (ImPlot::BeginPlot(("##graphPlot_" + column.name).c_str(),
                            ImVec2(-1, 100),
                            ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect |
                                ImPlotFlags_NoFrame)) {
                        ImPlot::SetupAxes(nullptr,
                            column.name.c_str(),
                            ImPlotAxisFlags_NoDecorations,
                            ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoTickLabels);
                        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, rangeResolution, ImPlotCond_Always);
                        // Disable labels for now
                        ImPlot::SetupAxisFormat(
                            ImAxis_X1,
                            [](double value, char *buff, int size, void *dataPtr) {
                                GraphDisplayGui &ctx = *(GraphDisplayGui *)dataPtr;
                                double timestampMs = (double)value * ctx.rangeSize / ctx.rangeResolution +
                                                     ctx.rangeStart;
                                return ImPlot::FormatDateTime(
                                    ImPlotTime((time_t)timestampMs / 1000, glm::fract(timestampMs / 1000) * 1e6),
                                    buff,
                                    size,
                                    ImPlotDateTimeSpec(ImPlotDateFmt_None, ImPlotTimeFmt_HrMinSMs, true));
                            },
                            this);
                        ImPlot::SetupFinish();
                        if (viewResolution == 0) {
                            viewResolution = ImPlot::GetPlotSize().x;
                            plotOffset.x = ImPlot::GetPlotPos().x;
                        }

                        ImVec4 col = ImPlot::GetColormapColor(0);
                        ImPlot::PushStyleColor(ImPlotCol_Line, col);
                        col.w = 0.3;
                        ImPlot::PushStyleColor(ImPlotCol_Fill, col);
                        ImPlot::PlotShadedG(("##graphShading_" + column.name).c_str(),
                            [](int i, void *dataPtr) {
                                ColumnData &column = *(ColumnData *)dataPtr;
                                return ImPlotPoint(i, column.data[i].min);
                            },
                            &column,
                            [](int i, void *dataPtr) {
                                ColumnData &column = *(ColumnData *)dataPtr;
                                return ImPlotPoint(i, column.data[i].max);
                            },
                            &column,
                            column.data.size());
                        ImPlot::PopStyleColor();
                        ImPlot::PlotLineG(("##graphLinesMin_" + column.name).c_str(),
                            [](int i, void *dataPtr) {
                                ColumnData &column = *(ColumnData *)dataPtr;
                                return ImPlotPoint(i, column.data[i].min);
                            },
                            &column,
                            column.data.size());
                        ImPlot::PlotLineG(("##graphLinesMax_" + column.name).c_str(),
                            [](int i, void *dataPtr) {
                                ColumnData &column = *(ColumnData *)dataPtr;
                                return ImPlotPoint(i, column.data[i].max);
                            },
                            &column,
                            column.data.size());
                        ImPlot::PopStyleColor();
                        ImPlot::EndPlot();
                    }
                    ImPlot::PopStyleVar(5);
                    if (ImGui::IsItemHovered() || ImGui::IsItemClicked()) {
                        selectedColumn = columnIndex;
                    }
                    // auto savedCursorPos = ImGui::GetCursorPos();
                    // ImGui::PlotLines(("##graphMin_" + column.name).c_str(),
                    //     [](void *dataPtr, int i) {
                    //         auto &data = *(std::vector<MinMaxSample> *)dataPtr;
                    //         return data[i].min;
                    //     },
                    //     &column.data,
                    //     column.data.size(),
                    //     0,
                    //     column.name.c_str(),
                    //     column.min - 0.1,
                    //     column.max + 0.1,
                    //     ImVec2(-1, 100));
                    // if (ImGui::IsItemHovered() || ImGui::IsItemClicked()) {
                    //     selectedColumn = columnIndex;
                    // }
                    // ImGui::SetCursorPos(savedCursorPos);
                    // ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
                    // ImGui::PlotLines(("##graphMax_" + column.name).c_str(),
                    //     [](void *dataPtr, int i) {
                    //         auto &data = *(std::vector<MinMaxSample> *)dataPtr;
                    //         return data[i].max;
                    //     },
                    //     &column.data,
                    //     column.data.size(),
                    //     0,
                    //     column.name.c_str(),
                    //     column.min - 0.1,
                    //     column.max + 0.1,
                    //     ImVec2(-1, 100));
                    // ImGui::PopStyleColor();
                }
            }
            ImGui::EndChild();

            auto &io = ImGui::GetIO();
            if (io.MouseDown[ImGuiMouseButton_Left]) {
                if (io.MousePos.y > plotOffset.y) {
                    size_t prevStart = viewStart;
                    viewStart -= io.MouseDelta.x / viewResolution * viewSize;
                    if ((prevStart & (1ull << 63)) != (viewStart & (1ull << 63))) {
                        // Don't allow wrap-around
                        viewStart = prevStart;
                    }
                    reloadingColumn = selectedColumn;
                }
            } else if (io.MouseReleased[ImGuiMouseButton_Left]) {
                reloadingColumn = 0;
                reloadingColumns = true;
            } else if (io.MouseDown[ImGuiMouseButton_Right]) {
                if (draggingView) {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, 0.2));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
                    ImGui::SetCursorScreenPos(ImVec2(std::min(io.MousePos.x, dragStartPos.x), plotOffset.y));
                    ImGui::BeginChild("dragArea",
                        ImVec2(std::abs(io.MousePos.x - dragStartPos.x) + 1, -1),
                        0,
                        ImGuiWindowFlags_NoInputs);
                    ImGui::Dummy(ImGui::GetContentRegionAvail());
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                } else if (io.MousePos.y >= plotOffset.y) {
                    dragStartPos = io.MousePos;
                    draggingView = true;
                }
            } else if (draggingView) {
                size_t startTimestamp = (dragStartPos.x - plotOffset.x) / viewResolution * viewSize + viewStart;
                size_t endTimestamp = (io.MousePos.x - plotOffset.x) / viewResolution * viewSize + viewStart;
                if (endTimestamp < startTimestamp) std::swap(startTimestamp, endTimestamp);
                viewStart = startTimestamp;
                viewSize = endTimestamp - startTimestamp;
                draggingView = false;
                reloadingColumn = 0;
                reloadingColumns = true;
            } else if (io.MousePos.y >= plotOffset.y) {
                // Draw cursor line
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, 0.2));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
                ImGui::SetCursorScreenPos(ImVec2(io.MousePos.x, plotOffset.y));
                ImGui::BeginChild("cursorLine", ImVec2(1, -1), 0, ImGuiWindowFlags_NoInputs);
                ImGui::Dummy(ImGui::GetContentRegionAvail());
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
            }

            if (!awaitingResponse && reloadingColumn >= 0) {
                sp_entity_t guiDefinitionEnt = sp_entity_ref_get(&guiDefinitionEntityRef, lock);
                sp_event_t eventOut2 = {"/csv/query_range", guiDefinitionEnt, {SP_EVENT_DATA_TYPE_VEC4}};
                eventOut2.data.vec4 = {(float)reloadingColumn,
                    (float)rangeResolution,
                    (float)rangeStart,
                    (float)rangeSize};
                sp_event_send_ref(lock, &dataEntityRef, &eventOut2);
                awaitingResponse = true;
            }
        }

        Tecs_lock_release(lock);

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
    }

    void PostDefine(sp_script_state_t *state, tecs_entity_t ent) {
        ImGui::PopFont();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }
};

extern "C" {

PLUGIN_EXPORT size_t sp_plugin_get_script_definitions(sp_dynamic_script_definition_t *output, size_t output_size) {
    if (output_size >= 2 && output != NULL) {
        std::strncpy(output[0].name, "signal_display2", sizeof(output[0].name) - 1);
        output[0].type = SP_SCRIPT_TYPE_GUI_SCRIPT;
        output[0].context_size = sizeof(ScriptGuiContext<SignalDisplayGui>);
        output[0].default_init_func = &ScriptGuiContext<SignalDisplayGui>::DefaultInit;
        output[0].init_func = &ScriptGuiContext<SignalDisplayGui>::Init;
        output[0].destroy_func = &ScriptGuiContext<SignalDisplayGui>::Destroy;
        output[0].before_frame_func = &ScriptGuiContext<SignalDisplayGui>::BeforeFrameStatic;
        output[0].render_gui_func = &ScriptGuiContext<SignalDisplayGui>::RenderGui;

        sp_struct_field_t *fields = sp_struct_field_vector_resize(&output[0].fields, 3);
        sp_string_set(&fields[0].name, "prefix");
        fields[0].type.type_index = SP_TYPE_INDEX_STRING;
        fields[0].size = sizeof(ScriptGuiContext<SignalDisplayGui>::prefix);
        fields[0].offset = offsetof(ScriptGuiContext<SignalDisplayGui>, prefix);
        sp_string_set(&fields[1].name, "suffix");
        fields[1].type.type_index = SP_TYPE_INDEX_STRING;
        fields[1].size = sizeof(ScriptGuiContext<SignalDisplayGui>::suffix);
        fields[1].offset = offsetof(ScriptGuiContext<SignalDisplayGui>, suffix);
        sp_string_set(&fields[2].name, "precision");
        fields[2].type.type_index = SP_TYPE_INDEX_UINT64;
        fields[2].size = sizeof(ScriptGuiContext<SignalDisplayGui>::precision);
        fields[2].offset = offsetof(ScriptGuiContext<SignalDisplayGui>, precision);

        std::strncpy(output[1].name, "signal_graph", sizeof(output[1].name) - 1);
        output[1].type = SP_SCRIPT_TYPE_GUI_SCRIPT;
        output[1].context_size = sizeof(ScriptGuiContext<GraphDisplayGui>);
        output[1].default_init_func = &ScriptGuiContext<GraphDisplayGui>::DefaultInit;
        output[1].init_func = &ScriptGuiContext<GraphDisplayGui>::Init;
        output[1].destroy_func = &ScriptGuiContext<GraphDisplayGui>::Destroy;
        output[1].before_frame_func = &ScriptGuiContext<GraphDisplayGui>::BeforeFrameStatic;
        output[1].render_gui_func = &ScriptGuiContext<GraphDisplayGui>::RenderGui;
        output[1].filter_on_event = false;
        event_name_t *events = sp_event_name_vector_resize(&output[1].events, 3);
        std::strncpy(events[0], "/csv/column_metadata", sizeof(events[0]) - 1);
        std::strncpy(events[1], "/csv/column_range", sizeof(events[1]) - 1);
        std::strncpy(events[2], "/csv/loading_progress", sizeof(events[2]) - 1);

        fields = sp_struct_field_vector_resize(&output[1].fields, 4);
        sp_string_set(&fields[0].name, "range_start");
        fields[0].type.type_index = SP_TYPE_INDEX_UINT64;
        fields[0].size = sizeof(ScriptGuiContext<GraphDisplayGui>::viewStart);
        fields[0].offset = offsetof(ScriptGuiContext<GraphDisplayGui>, viewStart);
        sp_string_set(&fields[1].name, "range_size");
        fields[1].type.type_index = SP_TYPE_INDEX_UINT64;
        fields[1].size = sizeof(ScriptGuiContext<GraphDisplayGui>::viewSize);
        fields[1].offset = offsetof(ScriptGuiContext<GraphDisplayGui>, viewSize);
        sp_string_set(&fields[2].name, "gui_definition_entity");
        fields[2].type.type_index = SP_TYPE_INDEX_ENTITY_REF;
        fields[2].size = sizeof(ScriptGuiContext<GraphDisplayGui>::guiDefinitionEntityRef);
        fields[2].offset = offsetof(ScriptGuiContext<GraphDisplayGui>, guiDefinitionEntityRef);
        sp_string_set(&fields[3].name, "data_entity");
        fields[3].type.type_index = SP_TYPE_INDEX_ENTITY_REF;
        fields[3].size = sizeof(ScriptGuiContext<GraphDisplayGui>::dataEntityRef);
        fields[3].offset = offsetof(ScriptGuiContext<GraphDisplayGui>, dataEntityRef);
    }
    return 2;
}

} // extern "C"
