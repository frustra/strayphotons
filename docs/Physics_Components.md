## Common Types

### `EntityRef` Type

An `EntityRef` is a stable reference to an entity via a string name. 

Referenced entities do not need to exist at the point an `EntityRef` is defined.
The reference will be automatically tracked and updated once the referenced entity is created.

Reference names are defined the same as the `name` component:
> *<scene_name>*:*<entity_name>*

References can also be defined relative to their entity scope, the same as a `name` component.
If just a relative name is provided, the reference will be expanded based on the scope root:
> *<scene_name>*:*<root_name>*.*<relative_name>*

The special "scoperoot" alias can be used to reference the parent entity during template generation.


### `Transform` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | Specifies the entity's position in 3D space. The +X direction represents Right, +Y represents Up, and -Z represents Forward. |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | Specifies the entity's orientation in 3D space. Multiple rotations can be automatically combined by specifying an array of rotations: `[[90, 1, 0, 0], [-90, 0, 1, 0]]` is equivalent to `[120, 1, -1, -1]` |
| **scale** | vec3 | [1, 1, 1] | Specifies the entity's size along each axis. A value of `[1, 1, 1]` leaves the size unchanged. If the scale is the same on all axes, a single scalar can be specified like `"scale": 0.5` |


## `physics` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **shapes** | vector&lt;`PhysicsShape`&gt; | [] | A list of individual shapes and models that combine to form the actor's overall collision shape. |
| **group** | enum `PhysicsGroup` | "World" | The collision group that this actor belongs to. |
| **type** | enum `PhysicsActorType` | "Dynamic" | "Dynamic" objects are affected by gravity, while Kinematic objects have an infinite mass and are only movable by game logic. "Static" objects are meant to be immovable and will not push objects if moved. The "SubActor" type adds this entity's shapes to the **parent_actor** entity instead of creating a new physics actor. |
| **parent_actor** | `EntityRef` | "" | Only used for "SubActor" type. If empty, the parent actor is determined by the `transform` parent. |
| **mass** | float | 0 | The weight of the physics actor in Kilograms (kg). Overrides **density** field. Only used for "Dynamic" objects. |
| **density** | float | 1000 | The density of the physics actor in Kilograms per Cubic Meter (kg/m^3). This value is ignored if **mass** != 0. Only used for "Dynamic" objects. |
| **angular_damping** | float | 0.05 | Resistance to changes in rotational velocity. Affects how quickly the entity will stop spinning. (>= 0.0) |
| **linear_damping** | float | 0 | Resistance to changes in linear velocity. Affects how quickly the entity will stop moving. (>= 0.0) |
| **contact_report_force** | float | -1 | The minimum collision force required to trigger a contact event. Force-based contact events are enabled if this value is >= 0.0 |
| **constant_force** | vec3 | [0, 0, 0] | A vector defining a constant force (in Newtons, N) that should be applied to the actor. The force vector is applied relative to the actor at its center of mass. |

### `PhysicsActorType` Type
Note: Enum string names are case-sensitive.
| Enum Value |
|------------|
| "Static" |
| "Dynamic" |
| "Kinematic" |
| "SubActor" |

### `PhysicsGroup` Type
Note: Enum string names are case-sensitive.
| Enum Value |
|------------|
| "NoClip" |
| "World" |
| "Interactive" |
| "HeldObject" |
| "Player" |
| "PlayerLeftHand" |
| "PlayerRightHand" |
| "UserInterface" |

### `PhysicsShape` Type

**See Also:**
`EntityRef`


## `physics_joints` Component
The `physics_joints` component has type: vector&lt;`PhysicsJoint`&gt;

### `PhysicsJoint` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **target** | `EntityRef` | "" | No description |
| **type** | enum `PhysicsJointType` | "Fixed" | No description |
| **limit** | vec2 | [0, 0] | No description |
| **local_offset** | `Transform` | {} | No description |
| **remote_offset** | `Transform` | {} | No description |

### `PhysicsJointType` Type
Note: Enum string names are case-sensitive.
| Enum Value |
|------------|
| "Fixed" |
| "Distance" |
| "Spherical" |
| "Hinge" |
| "Slider" |
| "Force" |
| "NoClip" |
| "TemporaryNoClip" |

**See Also:**
`EntityRef`
`Transform`


## `physics_query` Component
The `physics_query` component has no public fields


## `animation` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **states** | vector&lt;`AnimationState`&gt; | [] | No description |
| **interpolation** | enum `InterpolationMode` | "Linear" | No description |
| **cubic_tension** | float | 0.5 | No description |

Animations control the position of an entity by moving it between a set of animation states. Animation updates happen in the physics thread before each simulation step.
When an animation state is defined, the `transform` position is ignored except for the transform parent, using the pose from the animation.

Animations read and write two signal values:
1. **animation_state** - The current state index represented as a double from `0.0` to `N-1.0`.  
    A state value of `0.5` represents a state half way between states 0 and 1 based on transition time.
2. **animation_target** - The target state index. The entity will always animate towards this state.

The animation is running any time these values are different, and paused when they are equal.


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


### `InterpolationMode` Type
Note: Enum string names are case-sensitive.
| Enum Value |
|------------|
| "Step" |
| "Linear" |
| "Cubic" |


## `character_controller` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **head** | `EntityRef` | "" | No description |

**See Also:**
`EntityRef`


## `laser_emitter` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **intensity** | float | 1 | No description |
| **color** | vec3 (red, green, blue) | [0, 0, 0] | No description |
| **on** | bool | true | No description |
| **start_distance** | float | 0 | No description |


## `laser_sensor` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **threshold** | vec3 | [0.5, 0.5, 0.5] | No description |


## `trigger_area` Component
The `trigger_area` component has type: enum `TriggerShape`

### `TriggerShape` Type
Note: Enum string names are case-sensitive.
| Enum Value |
|------------|
| "Box" |
| "Sphere" |


## `trigger_group` Component
The `trigger_group` component has type: enum `TriggerGroup`

### `TriggerGroup` Type
Note: Enum string names are case-sensitive.
| Enum Value |
|------------|
| "Player" |
| "Object" |
| "Magnetic" |


## `scene_properties` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **root_transform** | `Transform` | {} | No description |
| **gravity_transform** | `Transform` | {} | No description |
| **gravity** | vec3 | [0, -9.81, 0] | No description |

**See Also:**
`Transform`


