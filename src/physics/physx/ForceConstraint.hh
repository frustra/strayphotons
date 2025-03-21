/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/components/Transform.h"

#include <PxPhysicsAPI.h>
#include <array>
#include <glm/glm.hpp>

namespace sp {
    using namespace physx;

    class ForceConstraint : public PxConstraintConnector {
    public:
        static const PxU32 TYPE_ID = PxConcreteType::eFIRST_USER_EXTENSION;

        struct Data {
            std::array<PxTransform, 2> c2b;
            float maxForce = 0.0f;
            float maxLiftForce = 0.0f;
            float maxTorque = 0.0f;
            glm::vec3 linearAccel = glm::vec3(0);
            glm::vec3 angularAccel = glm::vec3(0);
            glm::vec3 gravity = glm::vec3(0);
        };

        ForceConstraint(PxPhysics &physics,
            PxRigidActor *actor0,
            const PxTransform &localFrame0,
            PxRigidActor *actor1,
            const PxTransform &localFrame1);

        void release();

        void setActors(PxRigidActor *actor0, PxRigidActor *actor1);
        void setForceLimits(float maxForce, float maxLiftForce, float maxTorque);
        bool setLinearAccel(glm::vec3 linearAccel);
        bool setAngularAccel(glm::vec3 angularAccel);
        bool setGravity(glm::vec3 gravityAccel);

        void setLocalPose(PxJointActorIndex::Enum actor, const PxTransform &pose);
        PxTransform getLocalPose(PxJointActorIndex::Enum actor) const;

        void *prepareData() override;
        void onConstraintRelease() override;
        void onComShift(PxU32 actor) override;
        void onOriginShift(const PxVec3 &shift) override;
        void *getExternalReference(PxU32 &typeID) override;

        bool updatePvdProperties(pvdsdk::PvdDataStream &, const PxConstraint *, PxPvdUpdateType::Enum) const override {
            return true;
        }

        PxBase *getSerializable() override {
            return NULL;
        }

        PxConstraintSolverPrep getPrep() const override {
            return shaderTable.solverPrep;
        }

        const void *getConstantBlock() const override {
            return &data;
        }

    private:
        PxTransform getCenterOfMass(uint32_t index) const;
        PxTransform getCenterOfMass(PxRigidActor *actor) const;

        static PxU32 solverPrep(Px1DConstraint *constraints,
            PxVec3 &body0WorldOffset,
            PxU32 maxConstraints,
            PxConstraintInvMassScale &invMassScale,
            const void *constantBlock,
            const PxTransform &bA2w,
            const PxTransform &bB2w,
            bool useExtendedLimits,
            PxVec3 &cA2wOut,
            PxVec3 &cB2wOut);
        static void project(const void *constantBlock,
            PxTransform &bodyAToWorld,
            PxTransform &bodyBToWorld,
            bool projectToA);
        static void visualize(PxConstraintVisualizer &viz,
            const void *constantBlock,
            const PxTransform &body0Transform,
            const PxTransform &body1Transform,
            PxU32 flags);

        std::array<PxTransform, 2> localPoses;
        ecs::Transform targetTransform;

        PxConstraint *pxConstraint = nullptr;
        Data data = {};
        static PxConstraintShaderTable shaderTable;

        friend class ConstraintSystem;
    };
} // namespace sp
