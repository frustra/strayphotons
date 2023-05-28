## `SceneProperties` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **root_transform** | `Transform` | {} | No description |
| **gravity_transform** | `Transform` | {} | No description |
| **gravity** | vec3 | [0, -9.81, 0] | No description |


## `transform_snapshot` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | No description |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | No description |
| **scale** | vec3 | [1, 1, 1] | No description |


## `transform` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | No description |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | No description |
| **scale** | vec3 | [1, 1, 1] | No description |
| **parent** | `EntityRef` | "" | No description |


## `renderable` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **model** | string | "" | No description |
| **mesh_index** | size_t | 0 | No description |
| **visibility** | flags "DirectCamera\|DirectEye\|Transparent\|LightingShadow\|LightingVoxel\|Optics\|OutlineSelection" | "DirectCamera\|DirectEye\|LightingShadow\|LightingVoxel" | No description |
| **emissive** | float | 0 | No description |
| **color_override** | vec4 (red, green, blue, alpha) | [-1, -1, -1, -1] | No description |
| **metallic_roughness_override** | vec2 | [-1, -1] | No description |


## `physics` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **shapes** | vector&lt;`PhysicsShape`&gt; | [] | No description |
| **group** | enum "NoClip", "World", "Interactive", "HeldObject", "Player", "PlayerLeftHand", "PlayerRightHand", or "UserInterface" | "World" | No description |
| **type** | enum "Static", "Dynamic", "Kinematic", or "SubActor" | "Dynamic" | No description |
| **parent_actor** | `EntityRef` | "" | No description |
| **mass** | float | 0 | No description |
| **density** | float | 1000 | No description |
| **angular_damping** | float | 0.05 | No description |
| **linear_damping** | float | 0 | No description |
| **contact_report_force** | float | -1 | No description |
| **force** | vec3 | [0, 0, 0] | No description |


## `animation` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **states** | vector&lt;`AnimationState`&gt; | [] | No description |
| **interpolation** | enum "Step", "Linear", or "Cubic" | "Linear" | No description |
| **cubic_tension** | float | 0.5 | No description |


## `character_controller` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **head** | `EntityRef` | "" | No description |


## `gui` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **windowName** | string | "" | No description |
| **target** | enum "None", "World", or "Debug" | "World" | No description |


## `laser_emitter` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **intensity** | float | 1 | No description |
| **color** | vec3 (red, green, blue) | [0, 0, 0] | No description |
| **on** | bool | true | No description |
| **start_distance** | float | 0 | No description |


## `laser_line` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **intensity** | float | 1 | No description |
| **media_density** | float | 0.6 | No description |
| **on** | bool | true | No description |
| **relative** | bool | true | No description |
| **radius** | float | 0.003 | No description |


## `laser_sensor` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **threshold** | vec3 | [0.5, 0.5, 0.5] | No description |


## `light` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **intensity** | float | 0 | No description |
| **illuminance** | float | 0 | No description |
| **spotAngle** | float (degrees) | 0 | No description |
| **tint** | vec3 (red, green, blue) | [1, 1, 1] | No description |
| **gel** | string | "" | No description |
| **on** | bool | true | No description |
| **shadowMapSize** | uint32 | 9 | No description |
| **shadowMapClip** | vec2 | [0.1, 256] | No description |


## `light_sensor` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **position** | vec3 | [0, 0, 0] | No description |
| **direction** | vec3 | [0, 0, -1] | No description |
| **color_value** | vec3 | [0, 0, 0] | No description |


## `optic` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **pass_tint** | vec3 (red, green, blue) | [0, 0, 0] | No description |
| **reflect_tint** | vec3 (red, green, blue) | [1, 1, 1] | No description |
| **single_direction** | bool | false | No description |


## `physics_joints` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | vector&lt;`PhysicsJoint`&gt; | [] | No description |


## `physics_query` Component
This component has no public fields


## `scene_connection` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | map&lt;string, vector&lt;`SignalExpression`&gt;&gt; | {} | No description |


## `screen` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **target** | string | "" | No description |
| **luminance** | vec3 | [1, 1, 1] | No description |


## `sound` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | vector&lt;`Sound`&gt; | [] | No description |


## `trigger_area` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | enum "Box", or "Sphere" | "Box" | No description |


## `trigger_group` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | enum "Player", "Object", or "Magnetic" | "Player" | No description |


## `view` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **offset** | ivec2 | [0, 0] | No description |
| **extents** | ivec2 | [0, 0] | No description |
| **fov** | float (degrees) | 0 | No description |
| **clip** | vec2 | [0.1, 256] | No description |
| **visibilityMask** | flags "DirectCamera\|DirectEye\|Transparent\|LightingShadow\|LightingVoxel\|Optics\|OutlineSelection" | "" | No description |


## `voxel_area` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **extents** | ivec3 | [128, 128, 128] | No description |


## `xr_view` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | enum "Left", or "Right" | "Left" | No description |


## `event_input` Component
This component has no public fields


## `event_bindings` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | map&lt;string, vector&lt;`EventBinding`&gt;&gt; | {} | No description |


## `signal_output` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | map&lt;string, double&gt; | {} | No description |


## `signal_bindings` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | map&lt;string, `SignalExpression`&gt; | {} | No description |


## `script` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | vector&lt;`ScriptInstance`&gt; | [] | No description |


