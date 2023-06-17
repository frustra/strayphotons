
<div class="component_definition">

## Common Types

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

Signal expressions allow math and logic to be performed including almost any entity component property.
Expressions are defined as strings and automatically parsed and compiled for fast game logic evaluation.

A basic signal expression might look like this:  
`"(entity_name/signal_value + 1) > 10"`

The above will evaluate to `1` if the condition is true, or `0` if the condition is false.

Most "error" cases will evaulate to 0, such as an empty expression, missing referenced signals or entities, or division by 0.

Fields can be accessed on components using the following syntax:  
`"<entity_name>#<component_name>.<field_name>"`

For example: `light#renderable.emissive` will return the `emissive` value from the `light` entity's `renderable` component.

Vector fields such as position or color can be accessed as `pos.x` or `color.r`.

Note: Only number-compatble fields can be referenced. All evaluation is done using double floating point numbers.

</div>

</div>


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

The `collapse_events` script has parameter type: map&lt;string, string&gt;

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
| **input_expr** | string | "" | No description |
| **output_event** | string | "/script/edge_trigger" | No description |
| **falling_edge** | bool | true | No description |
| **rising_edge** | bool | true | No description |
| **init_value** | optional&lt;double&gt; | null | No description |
| **set_event_value** | optional&lt;[SignalExpression](#SignalExpression-type)&gt; | null | No description |

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `edit_tool` Script

The `edit_tool` script has no configurable parameters

</div>


<div class="component_definition">

## `elevator` Script

The `elevator` script has no configurable parameters

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

## `flashlight` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **parent** | [EntityRef](#EntityRef-type) | "" | No description |

**See Also:**
[EntityRef](#EntityRef-type)

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

**See Also:**
[EntityRef](#EntityRef-type)

</div>


<div class="component_definition">

## `interactive_object` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **disabled** | bool | false | No description |

</div>


<div class="component_definition">

## `life_cell` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **alive** | bool | false | No description |
| **initialized** | bool | false | No description |
| **neighbor_count** | int32 | 0 | No description |

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

**See Also:**
[EntityRef](#EntityRef-type)

</div>


<div class="component_definition">

## `physics_camera_view` Script

The `physics_camera_view` script has no configurable parameters

</div>


<div class="component_definition">

## `physics_collapse_events` Script

The `physics_collapse_events` script has parameter type: map&lt;string, string&gt;

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
| **input_expr** | string | "" | No description |
| **output_event** | string | "/script/edge_trigger" | No description |
| **falling_edge** | bool | true | No description |
| **rising_edge** | bool | true | No description |
| **init_value** | optional&lt;double&gt; | null | No description |
| **set_event_value** | optional&lt;[SignalExpression](#SignalExpression-type)&gt; | null | No description |

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

## `rotate_physics` Script

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

## `signal_from_event` Script

| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **outputs** | vector&lt;string&gt; | [] | No description |

</div>


<div class="component_definition">

## `sound_occlusion` Script

The `sound_occlusion` script has no configurable parameters

</div>


<div class="component_definition">

## `sun` Script

The `sun` script has no configurable parameters

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

