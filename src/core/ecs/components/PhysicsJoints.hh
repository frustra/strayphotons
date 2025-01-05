/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Transform.h"

#include <glm/glm.hpp>

namespace sp {
    class Gltf;
}

namespace physx {
    class PxJoint;
} // namespace physx

namespace ecs {
    enum class PhysicsJointType {
        // PhysX built-in joints
        Fixed = 0,
        Distance,
        Spherical,
        Hinge,
        Slider,

        // Custom joints
        Force, // Move actor0 to actor1 without exceeding force limits
        NoClip, // Prevents the 2 actors from colliding with eachother. Applies no forces.
        TemporaryNoClip, // Same as NoClip but removes itself once actors no longer overlap.
    };

    struct PhysicsJoint {
        EntityRef target;
        PhysicsJointType type = PhysicsJointType::Fixed;
        glm::vec2 limit = glm::vec2();
        Transform localOffset = Transform();
        Transform remoteOffset = Transform();

        bool operator==(const PhysicsJoint &) const = default;
    };

    static StructMetadata MetadataPhysicsJoint(typeid(PhysicsJoint),
        "PhysicsJoint",
        "",
        StructField::New("target", &PhysicsJoint::target),
        StructField::New("type", &PhysicsJoint::type),
        StructField::New("limit", &PhysicsJoint::limit),
        StructField::New("local_offset", &PhysicsJoint::localOffset),
        StructField::New("remote_offset", &PhysicsJoint::remoteOffset));

    struct PhysicsJoints {
        std::vector<PhysicsJoint> joints;

        void Add(const PhysicsJoint &joint) {
            if (!sp::contains(joints, joint)) joints.push_back(joint);
        }
    };

    static Component<PhysicsJoints> ComponentPhysicsJoints({typeid(PhysicsJoints),
        "physics_joints",
        "",
        StructField::New(&PhysicsJoints::joints, ~FieldAction::AutoApply)});

    template<>
    void Component<PhysicsJoints>::Apply(PhysicsJoints &dst, const PhysicsJoints &src, bool liveTarget);
} // namespace ecs
