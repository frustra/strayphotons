/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "NoClipConstraint.hh"

#include "physx/PhysxUtils.hh"

namespace sp {
    using namespace physx;

    PxConstraintShaderTable NoClipConstraint::shaderTable = {&NoClipConstraint::solverPrep,
        &NoClipConstraint::project,
        &NoClipConstraint::visualize,
        PxConstraintFlag::Enum(0)};

    NoClipConstraint::NoClipConstraint(PxPhysics &physics, PxRigidActor *actor0, PxRigidActor *actor1, bool temporary)
        : temporary(temporary) {
        pxConstraint = physics.createConstraint(actor0, actor1, *this, shaderTable, 0);
    }

    void NoClipConstraint::release() {
        pxConstraint->release();
    }

    void NoClipConstraint::setActors(PxRigidActor *actor0, PxRigidActor *actor1) {
        pxConstraint->setActors(actor0, actor1);
        pxConstraint->markDirty();
    }
} // namespace sp
