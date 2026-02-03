#pragma once

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS

// #define IMGUI_DISABLE

// #define IMGUI_DISABLE_DEMO_WINDOWS
// ShowDemoWindow()/ShowStyleEditor()/ImPlot::ShowDemoWindow()

// #define IMGUI_DISABLE_DEBUG_TOOLS
// Metrics/debugger and other debug tools: ShowMetricsWindow(), ShowDebugLogWindow() and ShowIDStackToolWindow()

// #define IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS
// [Win32] Don't implement default clipboard handler. Won't use and link with
// OpenClipboard/GetClipboardData/CloseClipboard etc. (user32.lib/.a, kernel32.lib/.a)

// #define IMGUI_ENABLE_WIN32_DEFAULT_IME_FUNCTIONS
// [Win32] [Default with Visual Studio] Implement default IME handler (require imm32.lib/.a, auto-link for Visual
// Studio, -limm32 on command-line for MinGW)

// #define IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
// [Win32] [Default with non-Visual Studio compilers] Don't implement default IME handler (won't require imm32.lib/.a)

// #define IMGUI_DISABLE_WIN32_FUNCTIONS
// [Win32] Won't use and link with any Win32 function (clipboard, IME).

// #define IMGUI_ENABLE_OSX_DEFAULT_CLIPBOARD_FUNCTIONS
// [OSX] Implement default OSX clipboard handler (need to link with '-framework ApplicationServices', this is why this
// is not the default).

// #define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
// Don't implement default platform_io.Platform_OpenInShellFn() handler (Win32: ShellExecute(), require shell32.lib/.a,
// Mac/Linux: use system("")).

// #define IMGUI_DISABLE_DEFAULT_FORMAT_FUNCTIONS
// Don't implement ImFormatString/ImFormatStringV so you can implement them yourself (e.g. if you don't want to link
// with vsnprintf)

// #define IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS
// Don't implement ImFabs/ImSqrt/ImPow/ImFmod/ImCos/ImSin/ImAcos/ImAtan2 so you can implement them yourself.

// #define IMGUI_DISABLE_FILE_FUNCTIONS
// Don't implement ImFileOpen/ImFileClose/ImFileRead/ImFileWrite and ImFileHandle at all (replace them with dummies)

// #define IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
// Don't implement ImFileOpen/ImFileClose/ImFileRead/ImFileWrite and ImFileHandle so you can implement them yourself if
// you don't want to link with fopen/fclose/fread/fwrite. This will also disable the LogToTTY() function.

// #define IMGUI_DISABLE_DEFAULT_ALLOCATORS
// Don't implement default allocators calling malloc()/free() to avoid linking with them. You will need to call
// ImGui::SetAllocatorFunctions().

// #define IMGUI_DISABLE_DEFAULT_FONT
// Disable default embedded font (ProggyClean.ttf), remove ~9.5 KB from output binary. AddFontDefault() will assert.

// #define IMGUI_DISABLE_SSE
// Disable use of SSE intrinsics even if available

//---- Avoid multiple STB libraries implementations, or redefine path/filenames to prioritize another version
// By default the embedded implementations are declared static and not available outside of Dear ImGui sources files.
// #define IMGUI_STB_TRUETYPE_FILENAME   "my_folder/stb_truetype.h"
// #define IMGUI_STB_RECT_PACK_FILENAME  "my_folder/stb_rect_pack.h"
// #define IMGUI_STB_SPRINTF_FILENAME    "my_folder/stb_sprintf.h"    // only used if IMGUI_USE_STB_SPRINTF is defined.
// #define IMGUI_DISABLE_STB_TRUETYPE_IMPLEMENTATION
// #define IMGUI_DISABLE_STB_RECT_PACK_IMPLEMENTATION
// #define IMGUI_DISABLE_STB_SPRINTF_IMPLEMENTATION                   // only disabled if IMGUI_USE_STB_SPRINTF is
// defined.

//---- Use stb_sprintf.h for a faster implementation of vsnprintf instead of the one from libc (unless
// IMGUI_DISABLE_DEFAULT_FORMAT_FUNCTIONS is defined)
// Compatibility checks of arguments and formats done by clang and GCC will be disabled in order to support the extra
// formats provided by stb_sprintf.h.
// #define IMGUI_USE_STB_SPRINTF

//---- Use FreeType to build and rasterize the font atlas (instead of stb_truetype which is embedded by default in Dear
// ImGui)
// Requires FreeType headers to be available in the include path. Requires program to be compiled with
// 'misc/freetype/imgui_freetype.cpp' (in this repository) + the FreeType library (not provided). On Windows you may use
// vcpkg with 'vcpkg install freetype --triplet=x64-windows' + 'vcpkg integrate install'.
// #define IMGUI_ENABLE_FREETYPE

//---- Use FreeType + plutosvg or lunasvg to render OpenType SVG fonts (SVGinOT)
// Only works in combination with IMGUI_ENABLE_FREETYPE.
// - plutosvg is currently easier to install, as e.g. it is part of vcpkg. It will support more fonts and may load them
// faster. See misc/freetype/README for instructions.
// - Both require headers to be available in the include path + program to be linked with the library code (not
// provided).
// - (note: lunasvg implementation is based on Freetype's rsvg-port.c which is licensed under CeCILL-C Free Software
// License Agreement)
// #define IMGUI_ENABLE_FREETYPE_PLUTOSVG
// #define IMGUI_ENABLE_FREETYPE_LUNASVG

//---- Use stb_truetype to build and rasterize the font atlas (default)
// The only purpose of this define is if you want force compilation of the stb_truetype backend ALONG with the FreeType
// backend.
// #define IMGUI_ENABLE_STB_TRUETYPE

//---- Define constructor and implicit cast operators to convert back<>forth between your math types and ImVec2/ImVec4.
// This will be inlined as part of ImVec2 and ImVec4 class declarations.

#include <glm/glm.hpp>

#define IM_VEC2_CLASS_EXTRA                                  \
    constexpr ImVec2(const glm::vec2 &f) : x(f.x), y(f.y) {} \
    operator glm::vec2() const {                             \
        return glm::vec2(x, y);                              \
    }

#define IM_VEC4_CLASS_EXTRA                                                  \
    constexpr ImVec4(const glm::vec4 &f) : x(f.x), y(f.y), z(f.z), w(f.w) {} \
    operator glm::vec4() const {                                             \
        return glm::vec4(x, y, z, w);                                        \
    }

//---- ...Or use Dear ImGui's own very basic math operators.
// #define IMGUI_DEFINE_MATH_OPERATORS

//---- Use 32-bit vertex indices (default is 16-bit) is one way to allow large meshes with more than 64K vertices.
// Your renderer backend will need to support it (most example renderer backends support both 16/32-bit indices).
// Another way to allow large meshes while keeping 16-bit indices is to handle ImDrawCmd::VtxOffset in your renderer.
// Read about ImGuiBackendFlags_RendererHasVtxOffset for details.
// #define ImDrawIdx unsigned int

//---- Override ImDrawCallback signature (will need to modify renderer backends accordingly)
// struct ImDrawList;
// struct ImDrawCmd;
// typedef void (*MyImDrawCallback)(const ImDrawList* draw_list, const ImDrawCmd* cmd, void* my_renderer_user_data);
// #define ImDrawCallback MyImDrawCallback

//---- Debug Tools: Macro to break in Debugger (we provide a default implementation of this in the codebase)
// (use 'Metrics->Tools->Item Picker' to pick widgets with the mouse and break into them for easy debugging.)
// #define IM_DEBUG_BREAK  IM_ASSERT(0)
// #define IM_DEBUG_BREAK  __debugbreak()

//---- Debug Tools: Enable slower asserts
// #define IMGUI_DEBUG_PARANOID

//---- Tip: You can add extra functions within the ImGui:: namespace from anywhere (e.g. your own sources/header files)
/*
namespace ImGui
{
    void MyFunction(const char* name, MyMatrix44* mtx);
}
*/

struct ImGuiContext;
extern thread_local ImGuiContext *GImGuiTLS;
#define GImGui GImGuiTLS
