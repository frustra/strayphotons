
<div class="component_definition">

## `physics` Component

| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **shapes** | vector&lt;[PhysicsShape](#PhysicsShape-type)&gt; | [] | A list of individual shapes and models that combine to form the actor's overall collision shape. |
| **group** | enum [PhysicsGroup](#PhysicsGroup-type) | "World" | The collision group that this actor belongs to. |
| **type** | enum [PhysicsActorType](#PhysicsActorType-type) | "Dynamic" | "Dynamic" objects are affected by gravity, while Kinematic objects have an infinite mass and are only movable by game logic. "Static" objects are meant to be immovable and will not push objects if moved. The "SubActor" type adds this entity's shapes to the **parent_actor** entity instead of creating a new physics actor. |
| **parent_actor** | [EntityRef](#EntityRef-type) | "" | Only used for "SubActor" type. If empty, the parent actor is determined by the `transform` parent. |
| **mass** | float | 0 | The weight of the physics actor in Kilograms (kg). Overrides **density** field. Only used for "Dynamic" objects. |
| **density** | float | 1000 | The density of the physics actor in Kilograms per Cubic Meter (kg/m^3). This value is ignored if **mass** != 0. Only used for "Dynamic" objects. |
| **angular_damping** | float | 0.05 | Resistance to changes in rotational velocity. Affects how quickly the entity will stop spinning. (>= 0.0) |
| **linear_damping** | float | 0 | Resistance to changes in linear velocity. Affects how quickly the entity will stop moving. (>= 0.0) |
| **contact_report_force** | float | -1 | The minimum collision force required to trigger a contact event. Force-based contact events are enabled if this value is >= 0.0 |
| **constant_force** | vec3 | [0, 0, 0] | A vector defining a constant force (in Newtons, N) that should be applied to the actor. The force vector is applied relative to the actor at its center of mass. |

<div class="type_definition">

### `PhysicsActorType` Type

A physics actor's type determines how it behaves in the world. The type should match the intended usage of an object. Dynamic actor's positions are taken over by the physics system, but scripts may still control these actors with physics joints or force-based constraints.

This is an **enum** type, and can be one of the following case-sensitive values:
- "**Static**" - The physics actor will not move. Used for walls, floors, and other static objects.
- "**Dynamic**" - The physics actor has a mass and is affected by gravity.
- "**Kinematic**" - The physics actor has infinite mass and is controlled by script or animation.
- "**SubActor**" - The shapes defined on this virtual physics actor are added to the parent physics actor instead.

</div>

<div class="type_definition">

### `PhysicsGroup` Type

An actor's physics group determines both what it will collide with in the physics simulation, and which physics queries it is visible to.

This is an **enum** type, and can be one of the following case-sensitive values:
- "**NoClip**" - Actors in this collision group will not collide with anything.
- "**World**" - This is the default collision group. All actors in this group will collide with eachother.
- "**Interactive**" - This group behaves like `World` but allows behavior to be customized for movable objects.
- "**HeldObject**" - Held objects do not collide with the player, but will collide with other held objects and the rest of the world.
- "**Player**" - This group is for the player's body, which collides with the world, but not other objects in any of the player groups.
- "**PlayerLeftHand**" - The player's left hand collides with the right hand, but not itself or the player's body.
- "**PlayerRightHand**" - The player's right hand collides with the left hand, but not itself or the player's body.
- "**UserInterface**" - This collision group is for popup UI elements that will only collide with the player's hands.

</div>

<div class="type_definition">

### `PhysicsShape` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **transform** | [Transform](#Transform-type) | {} | The position and orientation of the shape relative to the actor's origin (the entity transform position) |
| **static_friction** | float | 0.6 | This material's coefficient of static friction (>= 0.0) |
| **dynamic_friction** | float | 0.5 | This material's coefficient of dynamic friction (>= 0.0) |
| **restitution** | float | 0 | This material's coefficient of restitution (0.0 no bounce - 1.0 more bounce) |

Most physics shapes correlate with the underlying [PhysX Geometry Shapes](https://gameworksdocs.nvidia.com/PhysX/4.1/documentation/physxguide/Manual/Geometry.html).
The diagrams provided in the PhysX docs may be helpful in visualizing collisions.
Additionally an in-engine debug overlay can be turned on by entering `x.DebugColliders 1` in the consle.

A shape type is defined by setting one of the following additional fields:
| Shape Field | Type    | Default Value   | Description |
|-------------|---------|-----------------|-------------|
| **model**   | string  | ""              | Name of the cooked physics collision mesh to load |
| **plane**   | Plane   | {}              | Planes always face the +X axis relative to the actor |
| **capsule** | Capsule | {"radius": 0.5, "height": 1.0} | A capsule's total length along the X axis will be equal to `height + radius * 2` |
| **sphere**  | float   | 1.0             | Spheres are defined by their radius |
| **box**     | vec3    | [1.0, 1.0, 1.0] | Boxes define their dimensions by specifying the total length along the X, Y, and Z axes relative to the actor |

GLTF models automatically generate convex hull collision meshes.
They can be referenced by name in the form:  
`"<model_name>.convex<mesh_index>"`
e.g. `"box.convex0"`

If only a model name is specified, `convex0` will be used by default.

If a `model_name.physics.json` file is provided alongside the GLTF, then custom physics meshes can be generated and configured.
For example, the `duck.physics.json` physics definition defines `"duck.cooked"`,
which decomposes the duck model into multiple convex hulls to more accurately represent its non-convex shape.

</div>

**See Also:**
[EntityRef](#EntityRef-type)
[Transform](#Transform-type)

</div>


<div class="component_definition">

## `physics_joints` Component

The `physics_joints` component has type: vector&lt;[PhysicsJoint](#PhysicsJoint-type)&gt;

<div class="type_definition">

### `PhysicsJoint` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **target** | [EntityRef](#EntityRef-type) | "" | No description |
| **type** | enum [PhysicsJointType](#PhysicsJointType-type) | "Fixed" | No description |
| **limit** | vec2 | [0, 0] | No description |
| **local_offset** | [Transform](#Transform-type) | {} | No description |
| **remote_offset** | [Transform](#Transform-type) | {} | No description |

</div>

<div class="type_definition">

### `PhysicsJointType` Type
This is an **enum** type, and can be one of the following case-sensitive values:
- "**Fixed**" - No description
- "**Distance**" - No description
- "**Spherical**" - No description
- "**Hinge**" - No description
- "**Slider**" - No description
- "**Force**" - No description
- "**NoClip**" - No description
- "**TemporaryNoClip**" - No description

</div>

**See Also:**
[EntityRef](#EntityRef-type)
[Transform](#Transform-type)

</div>


<div class="component_definition">

## `physics_query` Component

The `physics_query` component has no public fields

</div>


<div class="component_definition">

## `animation` Component

Animations control the position of an entity by moving it between a set of animation states. Animation updates happen in the physics thread before each simulation step.
When an animation state is defined, the `transform` position is ignored except for the transform parent, using the pose from the animation.

Animations read and write two signal values:
1. **animation_state** - The current state index represented as a double from `0.0` to `N-1.0`.  
    A state value of `0.5` represents a state half way between states 0 and 1 based on transition time.
2. **animation_target** - The target state index. The entity will always animate towards this state.

The animation is running any time these values are different, and paused when they are equal.

| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **states** | vector&lt;[AnimationState](#AnimationState-type)&gt; | [] | No description |
| **interpolation** | enum [InterpolationMode](#InterpolationMode-type) | "Linear" | No description |
| **cubic_tension** | float | 0.5 | No description |

<div class="type_definition">

### `AnimationState` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **delay** | double | 0 | The time it takes to move to this state from any other state (in seconds) |
| **translate** | vec3 | [0, 0, 0] | A new position to override this entity's `transform` |
| **scale** | vec3 | [0, 0, 0] | A new scale to override this entity's `transform`. A scale of 0 will leave the transform unchanged. |
| **translate_tangent** | vec3 | [0, 0, 0] | Cubic interpolation tangent vector for **translate** (represents speed) |
| **scale_tangent** | vec3 | [0, 0, 0] | Cubic interpolation tangent vector for **scale** (represents rate of scaling) |

An example of a 3-state linear animation might look like this:
```json
"animation": {
    "states": [
        {
            "delay": 0.5,
            "translate": [0, 0, 0]
        },
        {
            "delay": 0.5,
            "translate": [0, 1, 0]
        },
        {
            "delay": 0.5,
            "translate": [0, 0, 1]
        }
    ]
}
```

When moving from state `2.0` to state `0.0`, the animation will follow the path through state `1.0`, rather than moving directly to the target position. The `animation_state` signal can however be manually controlled to teleport the animation to a specific state.

</div>

<div class="type_definition">

### `InterpolationMode` Type
This is an **enum** type, and can be one of the following case-sensitive values:
- "**Step**" - Teleport entities from state to state.
- "**Linear**" - Move entities at a constant speed between states.
- "**Cubic**" - Move entities according to a customizable Cubic Hermite spline curve.

</div>

</div>


<div class="component_definition">

## `character_controller` Component

| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **head** | [EntityRef](#EntityRef-type) | "" | No description |

**See Also:**
[EntityRef](#EntityRef-type)

</div>


<div class="component_definition">

## `laser_emitter` Component

| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **intensity** | float | 1 | No description |
| **color** | vec3 (red, green, blue) | [0, 0, 0] | No description |
| **on** | bool | true | No description |
| **start_distance** | float | 0 | No description |

</div>


<div class="component_definition">

## `laser_sensor` Component

| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **threshold** | vec3 | [0.5, 0.5, 0.5] | No description |

</div>


<div class="component_definition">

## `trigger_area` Component

The `trigger_area` component has type: enum [TriggerShape](#TriggerShape-type)

<div class="type_definition">

### `TriggerShape` Type
This is an **enum** type, and can be one of the following case-sensitive values:
- "**Box**" - No description
- "**Sphere**" - No description

</div>

</div>


<div class="component_definition">

## `trigger_group` Component

The `trigger_group` component has type: enum [TriggerGroup](#TriggerGroup-type)

<div class="type_definition">

### `TriggerGroup` Type
This is an **enum** type, and can be one of the following case-sensitive values:
- "**Player**" - No description
- "**Object**" - No description
- "**Magnetic**" - No description

</div>

</div>


<div class="component_definition">

## `scene_properties` Component

| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **root_transform** | [Transform](#Transform-type) | {} | No description |
| **gravity_transform** | [Transform](#Transform-type) | {} | No description |
| **gravity** | vec3 | [0, -9.81, 0] | No description |

**See Also:**
[Transform](#Transform-type)

</div>


<div class="component_definition">

## Additional Types

<div class="type_definition">

### `EntityRef` Type

An `EntityRef` is a stable reference to an entity via a string name. 

Referenced entities do not need to exist at the point an `EntityRef` is defined.
The reference will be automatically tracked and updated once the referenced entity is created.

Reference names are defined the same as the `name` component:  
`"<scene_name>:<entity_name>"`

References can also be defined relative to their entity scope, the same as a `name` component.
If just a relative name is provided, the reference will be expanded based on the scope root:  
`"<scene_name>:<root_name>.<relative_name>"`

The special `"scoperoot"` alias can be used to reference the parent entity during template generation.

</div>

<div class="type_definition">

### `Transform` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | Specifies the entity's position in 3D space. The +X direction represents Right, +Y represents Up, and -Z represents Forward. |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | Specifies the entity's orientation in 3D space. Multiple rotations can be combined by specifying an array of rotations: `[[90, 1, 0, 0], [-90, 0, 1, 0]]` is equivalent to `[120, 1, -1, -1]`. The rotation axis is automatically normalized. |
| **scale** | vec3 | [1, 1, 1] | Specifies the entity's size along each axis. A value of `[1, 1, 1]` leaves the size unchanged. If the scale is the same on all axes, a single scalar can be specified like `"scale": 0.5` |

</div>

</div>

