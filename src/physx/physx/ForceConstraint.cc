#include "ForceConstraint.hh"

#include "physx/PhysxUtils.hh"

namespace sp {
    using namespace physx;

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
        if (maxConstraints < 7) {
            Warnf("Not enough constraints available for force constraint: %u", maxConstraints);
            return 0;
        }

        const Data &data = *reinterpret_cast<const Data *>(constantBlock);
        auto cA2w = bA2w.transform(data.c2b[0]);
        auto cB2w = bB2w.transform(data.c2b[1]);
        body0WorldOffset = cB2w.p - bA2w.p;
        invMassScale = {1.0f, 1.0f, 1.0f, 1.0f};

        cA2wOut = cA2w.p;
        cB2wOut = cB2w.p;

        if (cA2w.q.dot(cB2w.q) < 0.0f) { // minimum distance quaternion
            cB2w.q = -cB2w.q;
        }

        glm::vec3 deltaPos = PxVec3ToGlmVec3(cB2w.p - cA2w.p);
        PxQuat deltaQuat = cA2w.q.getConjugate() * cB2w.q;
        static const glm::mat3 axes = glm::identity<glm::mat3>();
        glm::mat3 constraintAxes = glm::mat3_cast(PxQuatToGlmQuat(cA2w.q));

        auto *curr = constraints;
        for (size_t i = 0; i < 3; i++) {
            curr->flags |= Px1DConstraintFlag::eOUTPUT_FORCE;
            curr->linear0 = GlmVec3ToPxVec3(axes[i]);
            if (data.maxForce > 0.0f) {
                curr->flags |= PxU16(Px1DConstraintFlag::eSPRING | Px1DConstraintFlag::eACCELERATION_SPRING);
                curr->geometricError = -glm::dot(data.linearAccel, axes[i]);
                curr->mods.spring.stiffness = 1.0f;
                curr->mods.spring.damping = 0.0f;
                curr->minImpulse = -data.maxForce;
                curr->maxImpulse = data.maxForce;
            } else {
                curr->geometricError = -glm::dot(deltaPos, axes[i]);
            }
            curr++;

            curr->flags |= PxU16(Px1DConstraintFlag::eOUTPUT_FORCE | Px1DConstraintFlag::eANGULAR_CONSTRAINT);
            curr->angular0 = GlmVec3ToPxVec3(constraintAxes[i]);
            if (data.maxTorque > 0.0f) {
                curr->flags |= PxU16(Px1DConstraintFlag::eSPRING | Px1DConstraintFlag::eACCELERATION_SPRING);
                curr->geometricError = -glm::dot(data.angularAccel, axes[i]);
                curr->mods.spring.stiffness = 1.0f;
                curr->mods.spring.damping = 0.0f;
                curr->minImpulse = -data.maxTorque;
                curr->maxImpulse = data.maxTorque;
            } else {
                curr->geometricError = -glm::dot(PxVec3ToGlmVec3(deltaQuat.getImaginaryPart()), axes[i]);
            }
            curr++;
        }

        if (data.gravity != glm::vec3(0)) {
            curr->flags |= PxU16(Px1DConstraintFlag::eSPRING | Px1DConstraintFlag::eACCELERATION_SPRING |
                                 Px1DConstraintFlag::eOUTPUT_FORCE);
            curr->linear0 = GlmVec3ToPxVec3(-glm::normalize(data.gravity));
            curr->geometricError = -glm::length(data.gravity);
            curr->mods.spring.stiffness = 1.0f;
            curr->mods.spring.damping = 0.0f;
            curr->minImpulse = 0.0f;
            if (data.maxLiftForce > 0.0f) curr->maxImpulse = data.maxLiftForce;
            curr++;
        }

        return curr - constraints;
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

    void ForceConstraint::setForceLimits(float maxForce, float maxLiftForce, float maxTorque) {
        data.maxForce = maxForce;
        data.maxLiftForce = maxLiftForce;
        data.maxTorque = maxTorque;
        pxConstraint->markDirty();
    }

    bool ForceConstraint::setLinearAccel(glm::vec3 linearAccel) {
        if (data.linearAccel == linearAccel) return false;
        data.linearAccel = linearAccel;
        pxConstraint->markDirty();
        return true;
    }

    bool ForceConstraint::setAngularAccel(glm::vec3 angularAccel) {
        if (data.angularAccel == angularAccel) return false;
        data.angularAccel = angularAccel;
        pxConstraint->markDirty();
        return true;
    }

    bool ForceConstraint::setGravity(glm::vec3 gravityAccel) {
        if (data.gravity == gravityAccel) return false;
        data.gravity = gravityAccel;
        pxConstraint->markDirty();
        return true;
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
        if (actor && actor->is<PxRigidBody>()) {
            return static_cast<PxRigidBody *>(actor)->getCMassLocalPose();
        } else if (actor && actor->is<PxRigidStatic>()) {
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
