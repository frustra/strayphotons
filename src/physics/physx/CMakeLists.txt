#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

target_sources(${PROJECT_PHYSICS_PHYSX_LIB} PRIVATE
    AnimationSystem.cc
    CharacterControlSystem.cc
    ConstraintSystem.cc
    ForceConstraint.cc
    LaserSystem.cc
    NoClipConstraint.cc
    PhysicsDebugCommands.cc
    PhysicsQuerySystem.cc
    PhysxManager.cc
    SimulationCallbackHandler.cc
    TriggerSystem.cc
)
