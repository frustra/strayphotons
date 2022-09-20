#include "ForceConstraint.hh"

#include "console/CVar.hh"

namespace sp {
    using namespace physx;

    static CVar<float> CVarSpringStiffness("x.SpringStiffness", 100.0f, "");
    static CVar<float> CVarSpringDamping("x.SpringDamping", 5.0f, "");

    namespace detail {
        void computeJacobianAxes(PxVec3 row[3], const PxQuat &qa, const PxQuat &qb) {
            // Compute jacobian matrix for (qa* qb)  [[* means conjugate in this expr]]
            // d/dt (qa* qb) = 1/2 L(qa*) R(qb) (omega_b - omega_a)
            // result is L(qa*) R(qb), where L(q) and R(q) are left/right q multiply matrix

            const PxReal wa = qa.w, wb = qb.w;
            const PxVec3 va(qa.x, qa.y, qa.z), vb(qb.x, qb.y, qb.z);

            const PxVec3 c = vb * wa + va * wb;
            const PxReal d0 = wa * wb;
            const PxReal d1 = va.dot(vb);
            const PxReal d = d0 - d1;

            row[0] = (va * vb.x + vb * va.x + PxVec3(d, c.z, -c.y)) * 0.5f;
            row[1] = (va * vb.y + vb * va.y + PxVec3(-c.z, d, c.x)) * 0.5f;
            row[2] = (va * vb.z + vb * va.z + PxVec3(c.y, -c.x, d)) * 0.5f;

            if ((d0 + d1) != 0.0f) // check if relative rotation is 180 degrees which can lead to singular matrix
                return;
            else {
                row[0].x += PX_EPS_F32;
                row[1].y += PX_EPS_F32;
                row[2].z += PX_EPS_F32;
            }
        }

        Px1DConstraint *linear(const PxVec3 &axis,
            const PxVec3 &ra,
            const PxVec3 &rb,
            PxReal posErr,
            PxConstraintSolveHint::Enum hint,
            Px1DConstraint *c) {
            c->solveHint = PxU16(hint);
            c->linear0 = axis;
            c->angular0 = ra.cross(axis);
            c->linear1 = axis;
            c->angular1 = rb.cross(axis);
            c->geometricError = posErr;
            return c;
        }

        Px1DConstraint *angular(const PxVec3 &axis,
            PxReal posErr,
            PxConstraintSolveHint::Enum hint,
            Px1DConstraint *c) {
            c->solveHint = PxU16(hint);
            c->linear0 = PxVec3(0.0f);
            c->angular0 = axis;
            c->linear1 = PxVec3(0.0f);
            c->angular1 = axis;
            c->geometricError = posErr;
            c->flags |= Px1DConstraintFlag::eANGULAR_CONSTRAINT;
            return c;
        }
    } // namespace detail

    PxU32 ForceConstraint::solverPrep(Px1DConstraint *constraints,
        PxVec3 &body0WorldOffset,
        PxU32 maxConstraints,
        PxConstraintInvMassScale &invMassScale,
        const void *constantBlock,
        const PxTransform &bA2w,
        const PxTransform &bB2w,
        bool useExtendedLimits,
        PxVec3 &cA2wOut,
        PxVec3 &cB2wOut) {

        const Data &data = *reinterpret_cast<const Data *>(constantBlock);
        auto cA2w = bA2w.transform(data.c2b[0]);
        auto cB2w = bB2w.transform(data.c2b[1]);

        body0WorldOffset = cB2w.p - bA2w.p;
        invMassScale = data.invMassScale;

        auto ra = cB2w.p - bA2w.p;
        auto rb = cB2w.p - bB2w.p;

        auto mCA2w = cA2w.p;
        auto mCB2w = cB2w.p;

        if (cA2w.q.dot(cB2w.q) < 0.0f) // minimum dist quat (equiv to flipping cB2bB.q, which we don't use anywhere)
            cB2w.q = -cB2w.q;

        Px1DConstraint *current = constraints;
        auto cB2cAp = cA2w.transformInv(cB2w.p);

        PxVec3 errorVector(0.f);

        const PxMat33 axes(cA2w.q);

        errorVector -= axes * cB2cAp;

        ra += errorVector;

        detail::linear(axes.column0, ra, rb, -cB2cAp.x, PxConstraintSolveHint::eNONE, current++);
        detail::linear(axes.column1, ra, rb, -cB2cAp.y, PxConstraintSolveHint::eNONE, current++);
        detail::linear(axes.column2, ra, rb, -cB2cAp.z, PxConstraintSolveHint::eNONE, current++);

        const PxQuat qB2qA = cA2w.q.getConjugate() * cB2w.q;

        PxVec3 row[3];
        detail::computeJacobianAxes(row, cA2w.q, cB2w.q);
        detail::angular(row[0], -qB2qA.x, PxConstraintSolveHint::eNONE, current++);
        detail::angular(row[1], -qB2qA.y, PxConstraintSolveHint::eNONE, current++);
        detail::angular(row[2], -qB2qA.z, PxConstraintSolveHint::eNONE, current++);

        for (Px1DConstraint *c = constraints; c < current; c++) {
            c->flags |= Px1DConstraintFlag::eSPRING | Px1DConstraintFlag::eOUTPUT_FORCE;
            // F = stiffness * -geometricError + damping * (velocityTarget - v)
            c->mods.spring.stiffness = CVarSpringStiffness.Get();
            c->mods.spring.damping = CVarSpringDamping.Get();
        }

        cA2wOut = ra + bA2w.p;
        cB2wOut = rb + bB2w.p;

        return current - constraints;
    }

    void ForceConstraint::project(const void *constantBlock,
        PxTransform &bodyAToWorld,
        PxTransform &bodyBToWorld,
        bool projectToA) {}

    void ForceConstraint::visualize(PxConstraintVisualizer &viz,
        const void *constantBlock,
        const PxTransform &body0Transform,
        const PxTransform &body1Transform,
        PxU32 flags) {
        const Data &data = *reinterpret_cast<const Data *>(constantBlock);

        PxTransform cA2w = body0Transform * data.c2b[0];
        PxTransform cB2w = body1Transform * data.c2b[1];
        viz.visualizeJointFrames(cA2w, cB2w);
    }

    PxConstraintShaderTable ForceConstraint::shaderTable = {&ForceConstraint::solverPrep,
        &ForceConstraint::project,
        &ForceConstraint::visualize,
        PxConstraintFlag::Enum(0)};

    ForceConstraint::ForceConstraint(PxPhysics &physics,
        PxRigidActor *actor0,
        const PxTransform &localFrame0,
        PxRigidActor *actor1,
        const PxTransform &localFrame1) {
        pxConstraint = physics.createConstraint(actor0, actor1, *this, shaderTable, sizeof(Data));

        localPoses[0] = localFrame0.getNormalized();
        localPoses[1] = localFrame1.getNormalized();

        data.c2b[0] = getCenterOfMass(actor0).transformInv(localFrame0);
        data.c2b[1] = getCenterOfMass(actor1).transformInv(localFrame1);

        data.invMassScale.linear0 = 1.0f;
        data.invMassScale.angular0 = 1.0f;
        data.invMassScale.linear1 = 1.0f;
        data.invMassScale.angular1 = 1.0f;
    }

    void ForceConstraint::release() {
        pxConstraint->release();
    }

    void ForceConstraint::setActors(PxRigidActor *actor0, PxRigidActor *actor1) {
        pxConstraint->setActors(actor0, actor1);
        data.c2b[0] = getCenterOfMass(actor0).transformInv(localPoses[0]);
        data.c2b[1] = getCenterOfMass(actor1).transformInv(localPoses[1]);
        pxConstraint->markDirty();
    }

    void ForceConstraint::setForceLimits(float accelForce, float brakeForce) {
        data.accelForce = accelForce;
        data.brakeForce = brakeForce;
        pxConstraint->markDirty();
    }

    void ForceConstraint::setLocalPose(PxJointActorIndex::Enum actor, const PxTransform &pose) {
        localPoses[actor] = pose;
        data.c2b[actor] = getCenterOfMass(actor).transformInv(pose);
        pxConstraint->markDirty();
    }

    PxTransform ForceConstraint::getLocalPose(PxJointActorIndex::Enum actor) const {
        return localPoses[actor];
    }

    PxTransform ForceConstraint::getCenterOfMass(uint32_t index) const {
        PxRigidActor *a[2];
        pxConstraint->getActors(a[0], a[1]);
        return getCenterOfMass(a[index]);
    }

    PxTransform ForceConstraint::getCenterOfMass(PxRigidActor *actor) const {
        if (actor &&
            (actor->getType() == PxActorType::eRIGID_DYNAMIC || actor->getType() == PxActorType::eARTICULATION_LINK)) {
            return static_cast<PxRigidBody *>(actor)->getCMassLocalPose();
        } else if (actor && actor->getType() == PxActorType::eRIGID_STATIC) {
            return static_cast<PxRigidStatic *>(actor)->getGlobalPose().getInverse();
        }
        return PxTransform(PxIdentity);
    }

    void *ForceConstraint::prepareData() {
        return &data;
    }

    void ForceConstraint::onConstraintRelease() {
        delete this;
    }

    void ForceConstraint::onComShift(PxU32 actor) {
        data.c2b[actor] = getCenterOfMass(actor).transformInv(localPoses[actor]);
        pxConstraint->markDirty();
    }

    void ForceConstraint::onOriginShift(const PxVec3 &shift) {
        PxRigidActor *a[2];
        pxConstraint->getActors(a[0], a[1]);

        if (!a[0]) {
            localPoses[0].p -= shift;
            data.c2b[0].p -= shift;
            pxConstraint->markDirty();
        } else if (!a[1]) {
            localPoses[1].p -= shift;
            data.c2b[1].p -= shift;
            pxConstraint->markDirty();
        }
    }

    void *ForceConstraint::getExternalReference(PxU32 &typeID) {
        typeID = TYPE_ID;
        return this;
    }
} // namespace sp
