#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

target_sources(${PROJECT_SCRIPTS_LIB} PRIVATE
    AudioScripts.cc
    EditorScripts.cc
    EventScripts.cc
    InputScripts.cc
    InteractionScripts.cc
    Life.cc
    Magnets.cc
    MiscScripts.cc
    PhysicsScripts.cc
    VrHandScript.cc
)

add_executable(fusion_test FusionReactor.cc)
