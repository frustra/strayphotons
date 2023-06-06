#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

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
