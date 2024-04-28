/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <PxPhysicsAPI.h>

namespace sp {
    using namespace physx;

    class NoClipConstraint : public PxConstraintConnector {
    public:
        static const PxU32 TYPE_ID = PxConcreteType::eFIRST_USER_EXTENSION + 1;

        NoClipConstraint(PxPhysics &physics, PxRigidActor *actor0, PxRigidActor *actor1, bool temporary = false);

        void release();

        void setActors(PxRigidActor *actor0, PxRigidActor *actor1);

        void *prepareData() override {
            return nullptr;
        }

        void onConstraintRelease() override {
            delete this;
        }

        void onComShift(PxU32 actor) override {}
        void onOriginShift(const PxVec3 &shift) override {}

        void *getExternalReference(PxU32 &typeID) override {
            typeID = TYPE_ID;
            return this;
        }

        bool updatePvdProperties(pvdsdk::PvdDataStream &, const PxConstraint *, PxPvdUpdateType::Enum) const override {
            return true;
        }

        PxBase *getSerializable() override {
            return nullptr;
        }

        PxConstraintSolverPrep getPrep() const override {
            return shaderTable.solverPrep;
        }

        const void *getConstantBlock() const override {
            return nullptr;
        }

        bool temporary;

    private:
        static PxU32 solverPrep(Px1DConstraint *constraints,
            PxVec3 &body0WorldOffset,
            PxU32 maxConstraints,
            PxConstraintInvMassScale &invMassScale,
            const void *constantBlock,
            const PxTransform &bA2w,
            const PxTransform &bB2w,
            bool useExtendedLimits,
            PxVec3 &cA2wOut,
            PxVec3 &cB2wOut) {
            return 0;
        }

        static void project(const void *constantBlock,
            PxTransform &bodyAToWorld,
            PxTransform &bodyBToWorld,
            bool projectToA) {}

        static void visualize(PxConstraintVisualizer &viz,
            const void *constantBlock,
            const PxTransform &body0Transform,
            const PxTransform &body1Transform,
            PxU32 flags) {}

        PxConstraint *pxConstraint = nullptr;
        static PxConstraintShaderTable shaderTable;
    };
} // namespace sp
