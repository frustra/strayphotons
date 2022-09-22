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
        cA2wOut = bA2w.transform(data.c2b[0]).p;
        cB2wOut = bB2w.transform(data.c2b[1]).p;
        body0WorldOffset = cB2wOut - bA2w.p;
        invMassScale = {1.0f, 1.0f, 1.0f, 1.0f};

        static const std::array<glm::vec3, 3> axes = {glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)};

        auto *curr = constraints;
        for (auto &axis : axes) {
            curr->flags |= PxU16(Px1DConstraintFlag::eSPRING | Px1DConstraintFlag::eOUTPUT_FORCE);
            curr->linear0 = GlmVec3ToPxVec3(data.force * axis);
            curr->geometricError = -1.0f;
            curr->mods.spring.stiffness = 1.0f;
            curr->mods.spring.damping = 0.0f;
            if (data.maxForce > 0.0f) {
                curr->minImpulse = -data.maxForce;
                curr->maxImpulse = data.maxForce;
            }
            curr++;

            curr->flags |= PxU16(Px1DConstraintFlag::eSPRING | Px1DConstraintFlag::eOUTPUT_FORCE |
                                 Px1DConstraintFlag::eANGULAR_CONSTRAINT);
            curr->angular0 = GlmVec3ToPxVec3(data.torque * axis);
            curr->geometricError = -1.0f;
            curr->mods.spring.stiffness = 1.0f;
            curr->mods.spring.damping = 0.0f;
            if (data.maxTorque > 0.0f) {
                curr->minImpulse = -data.maxTorque;
                curr->maxImpulse = data.maxTorque;
            }
            curr++;
        }

        if (data.gravityForce != glm::vec3(0)) {
            curr->flags |= PxU16(Px1DConstraintFlag::eSPRING | Px1DConstraintFlag::eOUTPUT_FORCE);
            curr->linear0 = GlmVec3ToPxVec3(-glm::normalize(data.gravityForce));
            curr->geometricError = -glm::min(data.maxLiftForce, glm::length(data.gravityForce));
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

    bool ForceConstraint::setForce(glm::vec3 force) {
        if (data.force == force) return false;
        data.force = force;
        pxConstraint->markDirty();
        return true;
    }

    bool ForceConstraint::setTorque(glm::vec3 torque) {
        if (data.torque == torque) return false;
        data.torque = torque;
        pxConstraint->markDirty();
        return true;
    }

    bool ForceConstraint::setGravity(glm::vec3 gravityForce) {
        if (data.gravityForce == gravityForce) return false;
        data.gravityForce = gravityForce;
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
