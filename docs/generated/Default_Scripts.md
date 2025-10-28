
<div class="component_definition">

## `camera_view` Script

The `camera_view` script has no configurable parameters

</div>


<div class="component_definition">

## `charge_cell` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **discharging** | bool | false | No description |
| **charge_level** | double | 0 | No description |
| **max_charge_level** | double | 1 | No description |
| **output_power_r** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **output_power_g** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **output_power_b** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **charge_signal_r** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **charge_signal_g** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **charge_signal_b** | [SignalExpression](#SignalExpression-type) | "" | No description |

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `collapse_events` Script

The `collapse_events` script has parameter type: map&lt;string (max 127 chars), string&gt;

</div>


<div class="component_definition">

## `component_from_event` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **outputs** | vector&lt;string&gt; | [] | No description |

</div>


<div class="component_definition">

## `component_from_signal` Script

The `component_from_signal` script has parameter type: map&lt;string, [SignalExpression](#SignalExpression-type)&gt;

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `csv_visualizer` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **filename** | string | "" | No description |
| **current_time_ns** | size_t | 0 | No description |

</div>


<div class="component_definition">

## `debounce` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **delay_frames** | size_t | 1 | No description |
| **delay_ms** | size_t | 0 | No description |
| **input** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **output** | string | "" | No description |

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `edge_trigger` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **input_expr** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **output_event** | string | "/script/edge_trigger" | No description |
| **falling_edge** | bool | true | No description |
| **rising_edge** | bool | true | No description |
| **init_value** | optional&lt;double&gt; | null | No description |
| **set_event_value** | optional&lt;[SignalExpression](#SignalExpression-type)&gt; | null | No description |
| **_previous_value** | optional&lt;double&gt; | null | No description |

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `edit_tool` Script

The `edit_tool` script has no configurable parameters

</div>


<div class="component_definition">

## `event_from_signal` Script

The `event_from_signal` script has parameter type: map&lt;string, [SignalExpression](#SignalExpression-type)&gt;

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `event_gate_by_signal` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **input_event** | string | "" | No description |
| **output_event** | string | "" | No description |
| **gate_expr** | [SignalExpression](#SignalExpression-type) | "" | No description |

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `init_event` Script

The `init_event` script has parameter type: vector&lt;string&gt;

</div>


<div class="component_definition">

## `interact_handler` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **grab_distance** | float | 2 | No description |
| **noclip_entity** | [EntityRef](#EntityRef-type) | "" | No description |
| **_grab_entity** | [EntityRef](#EntityRef-type) | "" | No description |
| **_point_entity** | [EntityRef](#EntityRef-type) | "" | No description |
| **_press_entity** | [EntityRef](#EntityRef-type) | "" | No description |

**See Also:**
[EntityRef](#EntityRef-type)

</div>


<div class="component_definition">

## `interactive_object` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **disabled** | bool | false | No description |
| **highlight_only** | bool | false | No description |
| **_grab_entities** | vector&lt;pair&lt;[EntityRef](#EntityRef-type), [EntityRef](#EntityRef-type)&gt;&gt; | [] | No description |
| **_point_entities** | vector&lt;[EntityRef](#EntityRef-type)&gt; | [] | No description |
| **_render_outline** | bool | false | No description |

**See Also:**
[EntityRef](#EntityRef-type)

</div>


<div class="component_definition">

## `magnetic_plug` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **attach** | [EntityRef](#EntityRef-type) | "" | No description |
| **disabled** | bool | false | No description |

**See Also:**
[EntityRef](#EntityRef-type)

</div>


<div class="component_definition">

## `magnetic_socket` Script

The `magnetic_socket` script has no configurable parameters

</div>


<div class="component_definition">

## `model_spawner` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **relative_to** | [EntityRef](#EntityRef-type) | "" | No description |
| **position** | vec3 | [0, 0, 0] | No description |
| **model** | string | "" | No description |
| **templates** | vector&lt;string&gt; | "interactive" | No description |

**See Also:**
[EntityRef](#EntityRef-type)

</div>


<div class="component_definition">

## `physics_collapse_events` Script

The `physics_collapse_events` script has parameter type: map&lt;string (max 127 chars), string&gt;

</div>


<div class="component_definition">

## `physics_component_from_event` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **outputs** | vector&lt;string&gt; | [] | No description |

</div>


<div class="component_definition">

## `physics_component_from_signal` Script

The `physics_component_from_signal` script has parameter type: map&lt;string, [SignalExpression](#SignalExpression-type)&gt;

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `physics_debounce` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **delay_frames** | size_t | 1 | No description |
| **delay_ms** | size_t | 0 | No description |
| **input** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **output** | string | "" | No description |

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `physics_edge_trigger` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **input_expr** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **output_event** | string | "/script/edge_trigger" | No description |
| **falling_edge** | bool | true | No description |
| **rising_edge** | bool | true | No description |
| **init_value** | optional&lt;double&gt; | null | No description |
| **set_event_value** | optional&lt;[SignalExpression](#SignalExpression-type)&gt; | null | No description |
| **_previous_value** | optional&lt;double&gt; | null | No description |

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `physics_event_from_signal` Script

The `physics_event_from_signal` script has parameter type: map&lt;string, [SignalExpression](#SignalExpression-type)&gt;

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `physics_joint_from_event` Script

The `physics_joint_from_event` script has parameter type: map&lt;string, [PhysicsJoint](#PhysicsJoint-type)&gt;

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

<div class="type_definition">

### `Transform` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | Specifies the entity's position in 3D space. The +X direction represents Right, +Y represents Up, and -Z represents Forward. |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | Specifies the entity's orientation in 3D space. Multiple rotations can be combined by specifying an array of rotations: `[[90, 1, 0, 0], [-90, 0, 1, 0]]` is equivalent to `[120, 1, -1, -1]`. The rotation axis is automatically normalized. |
| **scale** | vec3 | [1, 1, 1] | Specifies the entity's size along each axis. A value of `[1, 1, 1]` leaves the size unchanged. If the scale is the same on all axes, a single scalar can be specified like `"scale": 0.5` |

</div>

**See Also:**
[EntityRef](#EntityRef-type)

</div>


<div class="component_definition">

## `physics_signal_from_signal` Script

The `physics_signal_from_signal` script has parameter type: map&lt;string, [SignalExpression](#SignalExpression-type)&gt;

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `physics_timer` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **names** | vector&lt;string&gt; | "timer" | No description |

</div>


<div class="component_definition">

## `player_rotation` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **relative_to** | [EntityRef](#EntityRef-type) | "" | No description |
| **smooth_rotation** | bool | false | No description |

**See Also:**
[EntityRef](#EntityRef-type)

</div>


<div class="component_definition">

## `prefab_gltf` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **model** | string | "" | No description |
| **skip_nodes** | vector&lt;string&gt; | [] | No description |
| **physics_group** | enum [PhysicsGroup](#PhysicsGroup-type) | "World" | No description |
| **render** | bool | true | No description |
| **physics** | optional&lt;enum [PhysicsActorType](#PhysicsActorType-type)&gt; | null | No description |
| **interactive** | bool | false | No description |

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

</div>


<div class="component_definition">

## `prefab_life_cell` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **board_size** | uvec2 | [32, 32] | No description |

</div>


<div class="component_definition">

## `prefab_template` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **source** | string | "" | No description |

</div>


<div class="component_definition">

## `prefab_tile` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **count** | ivec2 | [1, 1] | No description |
| **stride** | vec2 | [1, 1] | No description |
| **axes** | string | "xy" | No description |
| **surface** | string | "" | No description |
| **edge** | string | "" | No description |
| **corner** | string | "" | No description |

</div>


<div class="component_definition">

## `prefab_wall` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **y_offset** | float | 0 | No description |
| **stride** | float | 1 | No description |
| **segments** | vector&lt;vec2&gt; | [] | No description |
| **segment_types** | vector&lt;string&gt; | [] | No description |

</div>


<div class="component_definition">

## `relative_movement` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **relative_to** | [EntityRef](#EntityRef-type) | "" | No description |
| **up_reference** | [EntityRef](#EntityRef-type) | "" | No description |

**See Also:**
[EntityRef](#EntityRef-type)

</div>


<div class="component_definition">

## `rotate` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **axis** | vec3 | [0, 0, 0] | No description |
| **speed** | float | 0 | No description |

</div>


<div class="component_definition">

## `rotate_to_entity` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **up** | [EntityRef](#EntityRef-type) | "" | No description |
| **target** | [EntityRef](#EntityRef-type) | "" | No description |

**See Also:**
[EntityRef](#EntityRef-type)

</div>


<div class="component_definition">

## `signal_display` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **suffix** | string | "mW" | No description |

</div>


<div class="component_definition">

## `signal_from_event` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **outputs** | vector&lt;string&gt; | [] | No description |

</div>


<div class="component_definition">

## `signal_from_signal` Script

The `signal_from_signal` script has parameter type: map&lt;string, [SignalExpression](#SignalExpression-type)&gt;

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `sound_occlusion` Script

The `sound_occlusion` script has no configurable parameters

</div>


<div class="component_definition">

## `speed_controlled_sound` Script

The `speed_controlled_sound` script has no configurable parameters

</div>


<div class="component_definition">

## `timer` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **names** | vector&lt;string&gt; | "timer" | No description |

</div>


<div class="component_definition">

## `tray_spawner` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **source** | string | "" | No description |
| **reset_scale** | bool | false | No description |

</div>


<div class="component_definition">

## `volume_control` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **volume** | [SignalExpression](#SignalExpression-type) | "" | No description |

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `voxel_controller` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **voxel_scale** | float | 0.1 | No description |
| **voxel_stride** | float | 1 | No description |
| **voxel_offset** | vec3 | [0, 0, 0] | No description |
| **alignment_target** | [EntityRef](#EntityRef-type) | "" | No description |
| **follow_target** | [EntityRef](#EntityRef-type) | "" | No description |

**See Also:**
[EntityRef](#EntityRef-type)

</div>


<div class="component_definition">

## `vr_hand` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **hand** | string | "" | No description |
| **teleport_distance** | float | 2 | No description |
| **point_distance** | float | 2 | No description |
| **force_limit** | float | 100 | No description |
| **torque_limit** | float | 10 | No description |
| **noclip_entity** | [EntityRef](#EntityRef-type) | "" | No description |

**See Also:**
[EntityRef](#EntityRef-type)

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

### `SignalExpression` Type

Signal expressions allow math and logic to be performed using input from almost any entity property.  
Expressions are defined as strings and automatically parsed and compiled for fast game logic evaluation.

A basic signal expression might look like this:  
`"(entity/input_value + 1) > 10"`

The above will evaluate to `1.0` if the `input_value` signal on `entity` is greater than 9, or `0.0` otherwise.

> [!NOTE]
> All expressions are evaluated using double (64-bit) floating point numbers.  
> Whitespace is required before operators and function names.

Signal expressions support the following operations and functions:

- **Arithmetic operators**:
  - `a + b`: Addition
  - `a - b`: Subtraction
  - `a * b`: Multiplication
  - `a / b`: Division (Divide by zero returns 0.0)
  - `-a`: Sign Inverse
- **Boolean operators**: (Inputs are true if >= 0.5, output is `0.0` or `1.0`)
  - `a && b`: Logical AND
  - `a || b`: Logical OR
  - `!a`: Logical NOT
- **Comparison operators**: (Output is `0.0` or `1.0`)
  - `a > b`: Greater Than
  - `a >= b`: Greater Than or Equal
  - `a < b`: Less Than
  - `a <= b`: Less Than or Equal
  - `a == b`: Equal
  - `a != b`: Not Equal
- **Math functions**:
  - `sin(x)`, `cos(x)`, `tan(x)` (Input in radians)
  - `floor(x)`, `ceil(x)`, `abs(x)`
  - `min(a, b)`, `max(a, b)`
- **Focus functions**: (Possible focus layers: `Game`, `Menu`, `Overlay`)
  - `is_focused(FocusLayer)`: Returns `1.0` if the layer is active, else `0.0`.
  - `if_focused(FocusLayer, x)`: Returns `x` if the layer is active, else `0.0`.
- **Entity signal access**:
  - `<entity_name>/<signal_name>`: Read a signal on a specific entity. If the signal or entity is missing, returns `0.0`.
- **Component field access**:  
  - `"<entity_name>#<component_name>.<field_name>"`: Read a component value on a specific entity.  
    For example: `light#renderable.emissive` will return the `emissive` value from the `light` entity's `renderable` component.  
    Vector fields such as position or color can be accessed as `pos.x` or `color.r`.  
    **Note**: Only number-convertible fields can be referenced. Not all components are accessible from within the physics thread.

</div>

</div>

