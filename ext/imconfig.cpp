#include "imgui/imgui.h"

// See imconfig.h GImGui override.
// This allows multiple threads to have their own current context.
thread_local ImGuiContext *GImGuiTLS;
