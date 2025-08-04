#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

target_sources(${PROJECT_CORE_LIB} PRIVATE
    Components.cc
    DynamicLibrary.cc
    Ecs.cc
    EntityRef.cc
    EntityReferenceManager.cc
    EventQueue.cc
    ScriptManager.cc
    SignalExpression.cc
    SignalManager.cc
    SignalRef.cc
    SignalStructAccess.cc
    SignalStructAccess_read.cc
    SignalStructAccess_double.cc
    SignalStructAccess_vec2.cc
    SignalStructAccess_vec3.cc
    SignalStructAccess_vec4.cc
    StructMetadata.cc
)

# target_precompile_headers(${PROJECT_CORE_LIB} PRIVATE
#    Ecs.hh
#    EcsImpl.hh
#    EntityRef.hh
# )

add_subdirectory(components)
