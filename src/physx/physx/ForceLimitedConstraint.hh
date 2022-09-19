#pragma once

#include <PxPhysicsAPI.h>

namespace sp {
    using namespace physx;

    class ForceLimitedConstraint : public PxConstraintConnector {
    public:
        static const PxU32 TYPE_ID = PxConcreteType::eFIRST_USER_EXTENSION;

        struct Data {
            std::array<PxTransform, 2> c2b;
            float accelForce = 0.0f;
            float brakeForce = 0.0f;
        };

        ForceLimitedConstraint(PxPhysics &physics,
            PxRigidActor *actor0,
            const PxTransform &localFrame0,
            PxRigidActor *actor1,
            const PxTransform &localFrame1);

        void release();

        void setActors(PxRigidActor *actor0, PxRigidActor *actor1);
        void setForceLimits(float accelForce, float brakeForce);

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
        std::array<PxRigidBody *, 2> pxBodies;
        std::array<PxTransform, 2> localPoses;

        PxConstraint *pxConstraint = nullptr;
        Data data = {};
        static PxConstraintShaderTable shaderTable;
    };
} // namespace sp
