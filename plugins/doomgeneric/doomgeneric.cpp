/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "doomkeys.h"

#include <c_abi/Tecs.hh>
#include <c_abi/strayphotons_ecs_c_abi_entity_gen.h>
#include <c_abi/strayphotons_ecs_c_abi_lock_gen.h>
#include <common/InlineString.hh>
#include <common/Logging.hh>
#include <input/BindingNames.hh>
#include <input/KeyCodes.hh>
#include <strayphotons/components.h>
#include <vector>

extern "C" {
#include <doomgeneric/doomgeneric.h>
}

struct Image {
    std::vector<uint8_t> data;
    uint32_t width, height;
};

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;
static std::shared_ptr<Image> s_DoomImageBuffer, s_DoomStagingImageBuffer;
static chrono_clock::time_point s_StartTime;

static void addKeyToQueue(int pressed, sp::KeyCode keyCode) {
    unsigned char key = convertToDoomKey(keyCode);
    if (key == 0) return;

    unsigned short keyData = (pressed << 8) | key;

    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

struct ScriptContext {
    sp::InlineString<255> executableName;
    std::vector<sp::InlineString<255>> args;
    std::vector<char *> argv;
    bool hasFocus;

    ScriptContext() {}

    static void DefaultInit(void *context) {
        ScriptContext *ctx = static_cast<ScriptContext *>(context);
        new (ctx) ScriptContext();
    }

    static void Init(void *context, sp_script_state_t *state) {
        ScriptContext *ctx = static_cast<ScriptContext *>(context);
        ctx->executableName = "doomgeneric";
        ctx->argv.resize(ctx->args.size() + 1);
        ctx->argv[0] = ctx->executableName.data();
        for (size_t i = 1; i < ctx->argv.size(); i++) {
            ctx->argv[i] = ctx->args[i - 1].data();
        }
        s_StartTime = chrono_clock::now();
        doomgeneric_Create(ctx->argv.size(), ctx->argv.data());
    }

    static void Destroy(void *context, sp_script_state_t *state) {
        ScriptContext *ctx = static_cast<ScriptContext *>(context);
        *ctx = {};
    }

    static bool BeforeFrameStatic(void *,
        sp_compositor_ctx_t *compositor,
        sp_script_state_t *state,
        tecs_entity_t ent) {
        return true;
    }

    static void RenderGui(void *context,
        sp_compositor_ctx_t *compositor,
        sp_script_state_t *state,
        tecs_entity_t ent,
        vec2_t displaySize,
        vec2_t scale,
        float deltaTime,
        sp_gui_draw_data_t *result) {
        ScriptContext *ctx = static_cast<ScriptContext *>(context);

        tecs_lock_t *lock = Tecs_ecs_start_transaction(sp_get_live_ecs(),
            1 | SP_ACCESS_FOCUS_LOCK | SP_ACCESS_EVENT_INPUT,
            0);
        const sp_ecs_focus_lock_t *focusLock = sp_ecs_get_const_focus_lock(lock);
        ctx->hasFocus = sp_ecs_focus_lock_has_focus(focusLock, SP_FOCUS_LAYER_HUD);

        sp_event_t *event;
        while ((event = sp_script_state_poll_event(state, lock))) {
            if (strcmp(event->name, sp::INPUT_EVENT_MENU_KEY_DOWN.c_str()) == 0) {
                if (ctx->hasFocus && event->data.type == SP_EVENT_DATA_TYPE_INT) {
                    addKeyToQueue(1, (sp::KeyCode)event->data.i);
                }
            } else if (strcmp(event->name, sp::INPUT_EVENT_MENU_KEY_UP.c_str()) == 0) {
                if (ctx->hasFocus && event->data.type == SP_EVENT_DATA_TYPE_INT) {
                    addKeyToQueue(0, (sp::KeyCode)event->data.i);
                }
            }
        }

        Tecs_lock_release(lock);

        if (ctx->hasFocus) doomgeneric_Tick();

        if (compositor) {
            auto frameBuffer = s_DoomImageBuffer;
            if (frameBuffer && frameBuffer->width > 0 && frameBuffer->height > 0 && ctx->hasFocus) {
                sp_compositor_upload_source_image(compositor,
                    ent,
                    frameBuffer->data.data(),
                    frameBuffer->data.size(),
                    frameBuffer->width,
                    frameBuffer->height);
            } else {
                sp_compositor_clear_source_image(compositor, ent);
            }
        }
    }
};

extern "C" {

PLUGIN_EXPORT size_t sp_plugin_get_script_definitions(sp_dynamic_script_definition_t *output, size_t output_size) {
    if (output_size >= 1 && output != NULL) {
        std::strncpy(output[0].name, "doomgeneric", sizeof(output[0].name) - 1);
        output[0].type = SP_SCRIPT_TYPE_GUI_SCRIPT;
        output[0].context_size = sizeof(ScriptContext);
        output[0].default_init_func = &ScriptContext::DefaultInit;
        output[0].init_func = &ScriptContext::Init;
        output[0].destroy_func = &ScriptContext::Destroy;
        output[0].before_frame_func = &ScriptContext::BeforeFrameStatic;
        output[0].render_gui_func = &ScriptContext::RenderGui;
        output[0].filter_on_event = false;
        event_name_t *events = sp_event_name_vector_resize(&output[0].events, 2);
        std::strncpy(events[0], sp::INPUT_EVENT_MENU_KEY_DOWN.c_str(), sizeof(events[0]) - 1);
        std::strncpy(events[1], sp::INPUT_EVENT_MENU_KEY_UP.c_str(), sizeof(events[1]) - 1);

        sp_struct_field_t *fields = sp_struct_field_vector_resize(&output[0].fields, 1);
        sp_string_set(&fields[0].name, "cli_args");
        fields[0].type.type_index = SP_TYPE_INDEX_EVENT_STRING_VECTOR;
        fields[0].size = sizeof(ScriptContext::args);
        fields[0].offset = offsetof(ScriptContext, args);
    }
    return 1;
}

void DG_Init() {
    s_DoomImageBuffer = std::make_shared<Image>(Image{
        .data = std::vector<uint8_t>(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4),
        .width = DOOMGENERIC_RESX,
        .height = DOOMGENERIC_RESY,
    });
    s_DoomStagingImageBuffer = std::make_shared<Image>(Image{
        .data = std::vector<uint8_t>(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4),
        .width = DOOMGENERIC_RESX,
        .height = DOOMGENERIC_RESY,
    });
}

void DG_DrawFrame() {
    Assertf(s_DoomStagingImageBuffer, "s_DoomStagingImageBuffer is null");
    uint8_t *pixelData = s_DoomStagingImageBuffer->data.data();
    size_t pixelCount = s_DoomStagingImageBuffer->data.size() / 4;
    for (pixel_t *pixel = DG_ScreenBuffer; pixel < DG_ScreenBuffer + pixelCount; pixel++) {
        *pixelData++ = (uint8_t)((*pixel >> 16) & 0xFF);
        *pixelData++ = (uint8_t)((*pixel >> 8) & 0xFF);
        *pixelData++ = (uint8_t)(*pixel & 0xFF);
        *pixelData++ = 0xFF;
    }
    std::swap(s_DoomImageBuffer, s_DoomStagingImageBuffer);
}

void DG_SleepMs(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::microseconds(ms));
}

uint32_t DG_GetTicksMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(chrono_clock::now() - s_StartTime).count();
}

int DG_GetKey(int *pressed, unsigned char *doomKey) {
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) {
        // key queue is empty
        return 0;
    } else {
        unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
        s_KeyQueueReadIndex++;
        s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

        *pressed = keyData >> 8;
        *doomKey = keyData & 0xFF;

        return 1;
    }
}

void DG_SetWindowTitle(const char *title) {
    Logf("Setting window title to: %s", title);
}

} // extern "C"
