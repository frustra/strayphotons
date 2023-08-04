#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_module_sources(
    Game.cc
    Main.cc
)

add_executable(sp-link-test link_test.c)

target_link_libraries(sp-link-test PRIVATE sp)
