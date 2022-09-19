#include "ForceLimitedConstraint.hh"

namespace sp {
    using namespace physx;

    PxU32 solverPrep(Px1DConstraint *constraints,
        PxVec3 &body0WorldOffset,
        PxU32 maxConstraints,
        PxConstraintInvMassScale &,
        const void *constantBlock,
        const PxTransform &bA2w,
        const PxTransform &bB2w,
        bool /*useExtendedLimits*/,
        PxVec3 &cA2wOut,
        PxVec3 &cB2wOut) {
        const ForceLimitedConstraint::Data &data = *reinterpret_cast<const ForceLimitedConstraint::Data *>(
            constantBlock);

        PxTransform cA2w = bA2w.transform(data.c2b[0]);
        PxTransform cB2w = bB2w.transform(data.c2b[1]);

        cA2wOut = cA2w.p;
        cB2wOut = cB2w.p;

        body0WorldOffset = cB2w.p - bA2w.p;

        // compute geometric error:
        PxReal geometricError = (cA2w.p - cB2w.p).magnitude();

        Px1DConstraint *c = constraints;
        // constraint is breakable, so we need to output forces
        c->flags = Px1DConstraintFlag::eOUTPUT_FORCE;

        if (geometricError < 0.0f) {
            c->maxImpulse = PX_MAX_F32;
            c->minImpulse = 0;
            c->geometricError = geometricError;
        } else if (geometricError > 0.0f) {
            c->maxImpulse = 0;
            c->minImpulse = -PX_MAX_F32;
            c->geometricError = geometricError;
        }

        c->linear0 = PxVec3(0, 1, 0);
        c->angular0 = (cA2w.p - bA2w.p).cross(c->linear0);
        c->linear1 = PxVec3(0, -1, 0);
        c->angular1 = (cB2w.p - bB2w.p).cross(c->linear1);

        return 1;
    }

    void project(const void *constantBlock, PxTransform &bodyAToWorld, PxTransform &bodyBToWorld, bool projectToA) {
        PX_UNUSED(constantBlock);
        PX_UNUSED(bodyAToWorld);
        PX_UNUSED(bodyBToWorld);
        PX_UNUSED(projectToA);
    }

    void visualize(PxConstraintVisualizer &viz,
        const void *constantBlock,
        const PxTransform &body0Transform,
        const PxTransform &body1Transform,
        PxU32 flags) {
        PX_UNUSED(flags);
        const ForceLimitedConstraint::Data &data = *reinterpret_cast<const ForceLimitedConstraint::Data *>(
            constantBlock);

        PxTransform cA2w = body0Transform * data.c2b[0];
        PxTransform cB2w = body1Transform * data.c2b[1];
        viz.visualizeJointFrames(cA2w, cB2w);
    }

    PxConstraintShaderTable ForceLimitedConstraint::shaderTable = {&solverPrep,
        &project,
        &visualize,
        PxConstraintFlag::Enum(0)};

    ForceLimitedConstraint::ForceLimitedConstraint(PxPhysics &physics,
        PxRigidActor *actor0,
        const PxTransform &localFrame0,
        PxRigidActor *actor1,
        const PxTransform &localFrame1) {
        pxConstraint = physics.createConstraint(actor0, actor1, *this, shaderTable, sizeof(Data));

        pxBodies[0] = actor0->is<PxRigidBody>();
        pxBodies[1] = actor1->is<PxRigidBody>();

        localPoses[0] = localFrame0.getNormalized();
        localPoses[1] = localFrame1.getNormalized();

        data.c2b[0] = pxBodies[0]->getCMassLocalPose().transformInv(localPoses[0]);
        data.c2b[1] = pxBodies[1]->getCMassLocalPose().transformInv(localPoses[1]);
    }

    void ForceLimitedConstraint::release() {
        pxConstraint->release();
    }

    void ForceLimitedConstraint::setActors(PxRigidActor *actor0, PxRigidActor *actor1) {
        pxConstraint->setActors(actor0, actor1);
        data.c2b[0] = pxBodies[0]->getCMassLocalPose().transformInv(localPoses[0]);
        data.c2b[1] = pxBodies[1]->getCMassLocalPose().transformInv(localPoses[1]);
        pxConstraint->markDirty();
    }

    void ForceLimitedConstraint::setForceLimits(float accelForce, float brakeForce) {
        data.accelForce = accelForce;
        data.brakeForce = brakeForce;
        pxConstraint->markDirty();
    }

    void ForceLimitedConstraint::setLocalPose(PxJointActorIndex::Enum actor, const PxTransform &pose) {
        localPoses[actor] = pose;
        data.c2b[actor] = pxBodies[actor]->getCMassLocalPose().transformInv(pose);
        pxConstraint->markDirty();
    }

    PxTransform ForceLimitedConstraint::getLocalPose(PxJointActorIndex::Enum actor) const {
        return localPoses[actor];
    }

    void *ForceLimitedConstraint::prepareData() {
        return &data;
    }

    void ForceLimitedConstraint::onConstraintRelease() {
        delete this;
    }

    void ForceLimitedConstraint::onComShift(PxU32 actor) {
        data.c2b[actor] = pxBodies[actor]->getCMassLocalPose().transformInv(localPoses[actor]);
        pxConstraint->markDirty();
    }

    void ForceLimitedConstraint::onOriginShift(const PxVec3 &shift) {}

    void *ForceLimitedConstraint::getExternalReference(PxU32 &typeID) {
        typeID = TYPE_ID;
        return this;
    }
} // namespace sp
