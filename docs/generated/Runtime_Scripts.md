
<div class="script_definition">

## `camera_view` Script
The `camera_view` script has no configurable parameters

</div>

<div class="script_definition">

## `charge_cell` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **discharging** | bool | false | No description |
| **charge_level** | double | 0 | No description |
| **max_charge_level** | double | 1 | No description |
| **output_power_r** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **output_power_g** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **output_power_b** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **charge_signal_r** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **charge_signal_g** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **charge_signal_b** | [SignalExpression](#SignalExpression-type) | "" | No description |

</div>

<div class="script_definition">

## `collapse_events` Script
The `collapse_events` script has parameter type: map&lt;string, string&gt;

</div>

<div class="script_definition">

## `component_from_event` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **outputs** | vector&lt;string&gt; | [] | No description |

</div>

<div class="script_definition">

## `component_from_signal` Script
The `component_from_signal` script has parameter type: map&lt;string, [SignalExpression](#SignalExpression-type)&gt;

</div>

<div class="script_definition">

## `debounce` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **delay_frames** | size_t | 1 | No description |
| **delay_ms** | size_t | 0 | No description |
| **input** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **output** | string | "" | No description |

</div>

<div class="script_definition">

## `edge_trigger` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **input_expr** | string | "" | No description |
| **output_event** | string | "/script/edge_trigger" | No description |
| **falling_edge** | bool | true | No description |
| **rising_edge** | bool | true | No description |
| **init_value** | optional&lt;double&gt; | null | No description |
| **set_event_value** | optional&lt;[SignalExpression](#SignalExpression-type)&gt; | null | No description |

</div>

<div class="script_definition">

## `edit_tool` Script
The `edit_tool` script has no configurable parameters

</div>

<div class="script_definition">

## `elevator` Script
The `elevator` script has no configurable parameters

</div>

<div class="script_definition">

## `event_from_signal` Script
The `event_from_signal` script has parameter type: map&lt;string, [SignalExpression](#SignalExpression-type)&gt;

</div>

<div class="script_definition">

## `event_gate_by_signal` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **input_event** | string | "" | No description |
| **output_event** | string | "" | No description |
| **gate_expr** | [SignalExpression](#SignalExpression-type) | "" | No description |

</div>

<div class="script_definition">

## `flashlight` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **parent** | [EntityRef](#EntityRef-type) | "" | No description |

</div>

<div class="script_definition">

## `init_event` Script
The `init_event` script has parameter type: vector&lt;string&gt;

</div>

<div class="script_definition">

## `interact_handler` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **grab_distance** | float | 2 | No description |
| **noclip_entity** | [EntityRef](#EntityRef-type) | "" | No description |

</div>

<div class="script_definition">

## `interactive_object` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **disabled** | bool | false | No description |

</div>

<div class="script_definition">

## `life_cell` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **alive** | bool | false | No description |
| **initialized** | bool | false | No description |
| **neighbor_count** | int32 | 0 | No description |

</div>

<div class="script_definition">

## `magnetic_plug` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **attach** | [EntityRef](#EntityRef-type) | "" | No description |
| **disabled** | bool | false | No description |

</div>

<div class="script_definition">

## `magnetic_socket` Script
The `magnetic_socket` script has no configurable parameters

</div>

<div class="script_definition">

## `model_spawner` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **relative_to** | [EntityRef](#EntityRef-type) | "" | No description |
| **position** | vec3 | [0, 0, 0] | No description |
| **model** | string | "" | No description |

</div>

<div class="script_definition">

## `physics_camera_view` Script
The `physics_camera_view` script has no configurable parameters

</div>

<div class="script_definition">

## `physics_collapse_events` Script
The `physics_collapse_events` script has parameter type: map&lt;string, string&gt;

</div>

<div class="script_definition">

## `physics_component_from_event` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **outputs** | vector&lt;string&gt; | [] | No description |

</div>

<div class="script_definition">

## `physics_component_from_signal` Script
The `physics_component_from_signal` script has parameter type: map&lt;string, [SignalExpression](#SignalExpression-type)&gt;

</div>

<div class="script_definition">

## `physics_debounce` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **delay_frames** | size_t | 1 | No description |
| **delay_ms** | size_t | 0 | No description |
| **input** | [SignalExpression](#SignalExpression-type) | "" | No description |
| **output** | string | "" | No description |

</div>

<div class="script_definition">

## `physics_edge_trigger` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **input_expr** | string | "" | No description |
| **output_event** | string | "/script/edge_trigger" | No description |
| **falling_edge** | bool | true | No description |
| **rising_edge** | bool | true | No description |
| **init_value** | optional&lt;double&gt; | null | No description |
| **set_event_value** | optional&lt;[SignalExpression](#SignalExpression-type)&gt; | null | No description |

</div>

<div class="script_definition">

## `physics_event_from_signal` Script
The `physics_event_from_signal` script has parameter type: map&lt;string, [SignalExpression](#SignalExpression-type)&gt;

</div>

<div class="script_definition">

## `physics_joint_from_event` Script
The `physics_joint_from_event` script has parameter type: map&lt;string, [PhysicsJoint](#PhysicsJoint-type)&gt;

</div>

<div class="script_definition">

## `physics_timer` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **names** | vector&lt;string&gt; | "timer" | No description |

</div>

<div class="script_definition">

## `player_rotation` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **relative_to** | [EntityRef](#EntityRef-type) | "" | No description |
| **smooth_rotation** | bool | false | No description |

</div>

<div class="script_definition">

## `relative_movement` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **relative_to** | [EntityRef](#EntityRef-type) | "" | No description |
| **up_reference** | [EntityRef](#EntityRef-type) | "" | No description |

</div>

<div class="script_definition">

## `rotate` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **axis** | vec3 | [0, 0, 0] | No description |
| **speed** | float | 0 | No description |

</div>

<div class="script_definition">

## `rotate_physics` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **axis** | vec3 | [0, 0, 0] | No description |
| **speed** | float | 0 | No description |

</div>

<div class="script_definition">

## `rotate_to_entity` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **up** | [EntityRef](#EntityRef-type) | "" | No description |
| **target** | [EntityRef](#EntityRef-type) | "" | No description |

</div>

<div class="script_definition">

## `signal_from_event` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **outputs** | vector&lt;string&gt; | [] | No description |

</div>

<div class="script_definition">

## `sound_occlusion` Script
The `sound_occlusion` script has no configurable parameters

</div>

<div class="script_definition">

## `sun` Script
The `sun` script has no configurable parameters

</div>

<div class="script_definition">

## `timer` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **names** | vector&lt;string&gt; | "timer" | No description |

</div>

<div class="script_definition">

## `tray_spawner` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **source** | string | "" | No description |

</div>

<div class="script_definition">

## `voxel_controller` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **voxel_scale** | float | 0.1 | No description |
| **voxel_stride** | float | 1 | No description |
| **voxel_offset** | vec3 | [0, 0, 0] | No description |
| **alignment_target** | [EntityRef](#EntityRef-type) | "" | No description |
| **follow_target** | [EntityRef](#EntityRef-type) | "" | No description |

</div>

<div class="script_definition">

## `vr_hand` Script
| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| **hand** | string | "" | No description |
| **teleport_distance** | float | 2 | No description |
| **point_distance** | float | 2 | No description |
| **force_limit** | float | 100 | No description |
| **torque_limit** | float | 10 | No description |
| **noclip_entity** | [EntityRef](#EntityRef-type) | "" | No description |

</div>
