#
# Stray Photons - Copyright (C) 2025 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(ImPlot STATIC
    implot/implot.cpp
    implot/implot_items.cpp
    implot/implot_demo.cpp
)

target_include_directories(ImPlot PUBLIC
    ./implot
    ./include/imgui
)

target_link_libraries(ImPlot PUBLIC ImGui)

target_compile_definitions(ImPlot PUBLIC IMGUI_USER_CONFIG="custom_imconfig.h")
