
add_library(
    ImGui 
    STATIC 
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_widgets.cpp
        imgui/imgui_tables.cpp
        imgui/misc/cpp/imgui_stdlib.cpp
        imconfig.cpp
)

target_include_directories(ImGui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
