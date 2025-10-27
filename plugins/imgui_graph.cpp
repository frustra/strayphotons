/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <c_abi/Tecs.hh>
#include <c_abi/strayphotons_ecs_c_abi_entity_gen.h>
#include <c_abi/strayphotons_ecs_c_abi_lock_gen.h>
#include <common/Logging.hh>
#include <fstream>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <strayphotons/components.h>

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

struct SignalDisplayGui {
    std::string suffix = "mW";
    ImGuiContext *imCtx = nullptr;
    shared_ptr<ImFontAtlas> fontAtlas;
    ImFontAtlas *originalAtlas = nullptr;

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

    bool PreDefine(sp_script_state_t *state, tecs_entity_t ent) {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        PushFont(sp::GuiFont::Monospace, 32);
        return true;
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
            double value = sp_signal_ref_get_signal(&valueRef, lock, 0) * ImGui::GetIO().DeltaTime * 1000;

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
                ss << std::fixed << std::setprecision(2) << (value / maxValue * 100.0) << "%";
            } else {
                ss << std::fixed << std::setprecision(2) << value << suffix;
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

    static void DefaultInit(void *context) {
        SignalDisplayGui *ctx = static_cast<SignalDisplayGui *>(context);
        *ctx = SignalDisplayGui();
    }

    static void Init(void *context, sp_script_state_t *state) {
        SignalDisplayGui *ctx = static_cast<SignalDisplayGui *>(context);
        sp_event_name_vector_resize(&state->definition.events, 1);
        event_name_t *events = sp_event_name_vector_get_data(&state->definition.events);
        std::strncpy(events[0], "/gui/imgui_input", sizeof(events[0]) - 1);

        ctx->imCtx = ImGui::CreateContext();
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
                strncpy_s(cfg.Name,
                    sizeof(cfg.Name),
                    filename.c_str(),
                    std::min(sizeof(cfg.Name) - 1, filename.length()));
                // memcpy(cfg.Name, filename.c_str(), std::min(sizeof(cfg.Name), filename.length()));
                ctx->fontAtlas->AddFont(&cfg);
            }
        }

        uint8 *fontData;
        int fontWidth, fontHeight;
        ctx->fontAtlas->GetTexDataAsRGBA32(&fontData, &fontWidth, &fontHeight);

        ImGuiIO &io = ImGui::GetIO();
        ctx->originalAtlas = io.Fonts;
        io.Fonts = ctx->fontAtlas.get();
        io.Fonts->TexID = ~(ImTextureID)0; // FONT_ATLAS_ID
        io.IniFilename = nullptr;
    }

    static void Destroy(void *context, sp_script_state_t *state) {
        SignalDisplayGui *ctx = static_cast<SignalDisplayGui *>(context);
        ImGui::GetIO().Fonts = ctx->originalAtlas;
        ImGui::DestroyContext(ctx->imCtx);
        ctx->imCtx = nullptr;
    }

    static void BeforeFrame(void *context, sp_script_state_t *state, tecs_lock_t *lock, tecs_entity_t ent) {
        SignalDisplayGui *ctx = static_cast<SignalDisplayGui *>(context);
        sp_event_t *event;
        while ((event = sp_script_state_poll_event(state, lock))) {
            if (strcmp(event->name, "/gui/imgui_input") != 0) continue;
            if (event->data.type != SP_EVENT_DATA_TYPE_BYTES) continue;

            ImGuiInputEvent inputEvent = reinterpret_cast<const ImGuiInputEvent &>(event->data.bytes);
            inputEvent.EventId = ctx->imCtx->InputEventsNextEventId++;
            ctx->imCtx->InputEventsQueue.push_back(inputEvent);
        }
    }

    static ImDrawData *RenderGui(void *context,
        sp_script_state_t *state,
        tecs_entity_t ent,
        vec2_t displaySize,
        vec2_t scale,
        float deltaTime) {
        SignalDisplayGui *ctx = static_cast<SignalDisplayGui *>(context);
        if (!ctx->imCtx) return nullptr;
        ImGui::SetCurrentContext(ctx->imCtx);
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2(displaySize.v[0], displaySize.v[1]);
        io.DisplayFramebufferScale = ImVec2(scale.v[0], scale.v[1]);
        io.DeltaTime = deltaTime;

        ImGui::NewFrame();

        // DefineWindows
        int flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;

        if (ctx->PreDefine(state, ent)) {
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
            ImGui::Begin("signal_display", nullptr, flags);
            ctx->DefineContents(state, ent);
            ImGui::End();
            ctx->PostDefine(state, ent);
        }

        ImGui::Render();

        return ImGui::GetDrawData();
    }
};

extern "C" {

PLUGIN_EXPORT size_t sp_plugin_get_script_definitions(sp_dynamic_script_definition_t *output, size_t output_size) {
    if (output_size >= 1 && output != NULL) {
        std::strncpy(output[0].name, "signal_display2", sizeof(output[0].name) - 1);
        output[0].type = SP_SCRIPT_TYPE_GUI_SCRIPT;
        output[0].context_size = sizeof(SignalDisplayGui);
        output[0].default_init_func = &SignalDisplayGui::DefaultInit;
        output[0].init_func = &SignalDisplayGui::Init;
        output[0].destroy_func = &SignalDisplayGui::Destroy;
        output[0].before_frame_func = &SignalDisplayGui::BeforeFrame;
        output[0].render_gui_func = &SignalDisplayGui::RenderGui;

        sp_struct_field_t *fields = sp_struct_field_vector_resize(&output[0].fields, 1);
        sp_string_set(&fields[0].name, "suffix");
        fields[0].type.type_index = SP_TYPE_INDEX_STRING;
        fields[0].size = sizeof(SignalDisplayGui::suffix);
        fields[0].offset = offsetof(SignalDisplayGui, suffix);
    }
    return 1;
}

} // extern "C"
