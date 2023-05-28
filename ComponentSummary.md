## `SceneProperties` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **root_transform** | `Transform` | null | No description |
| **gravity_transform** | `Transform` | {"gravity_transform":{"rotate":[0, 0, 0, 1],"scale":[1, 1, 1],"translate":[0, 0, 0]}} | No description |
| **gravity** | vec3 | {"gravity":[0, -9.81, 0]} | No description |


## `transform_snapshot` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | null | No description |
| **scale** | vec3 | null | No description |


## `transform` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | null | No description |
| **scale** | vec3 | null | No description |
| **parent** | `EntityRef` | null | No description |


## `renderable` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **model** | string | {"model":""} | No description |
| **mesh_index** | size_t | {"mesh_index":0} | No description |
| **visibility** | flags "DirectCamera|DirectEye|Transparent|LightingShadow|LightingVoxel|Optics|OutlineSelection" | {"visibility":"DirectCamera|DirectEye|LightingShadow|LightingVoxel"} | No description |
| **emissive** | float | {"emissive":0} | No description |
| **color_override** | vec4 (red, green, blue, alpha) | {"color_override":[-1, -1, -1, -1]} | No description |
| **metallic_roughness_override** | vec2 | {"metallic_roughness_override":[-1, -1]} | No description |


## `physics` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **shapes** | vector<`PhysicsShape`> | {"shapes":[]} | No description |
| **group** | enum "NoClip", "World", "Interactive", "HeldObject", "Player", "PlayerLeftHand", "PlayerRightHand", or "UserInterface" | {"group":"World"} | No description |
| **type** | enum "Static", "Dynamic", "Kinematic", or "SubActor" | {"type":"Dynamic"} | No description |
| **parent_actor** | `EntityRef` | null | No description |
| **mass** | float | {"mass":0} | No description |
| **density** | float | {"density":1000} | No description |
| **angular_damping** | float | {"angular_damping":0.05} | No description |
| **linear_damping** | float | {"linear_damping":0} | No description |
| **contact_report_force** | float | {"contact_report_force":-1} | No description |
| **force** | vec3 | {"force":[0, 0, 0]} | No description |


## `animation` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **states** | vector<`AnimationState`> | {"states":[]} | No description |
| **interpolation** | enum "Step", "Linear", or "Cubic" | {"interpolation":"Linear"} | No description |
| **cubic_tension** | float | {"cubic_tension":0.5} | No description |


## `character_controller` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **head** | `EntityRef` | null | No description |


## `gui` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **windowName** | string | {"windowName":""} | No description |
| **target** | enum "None", "World", or "Debug" | {"target":"World"} | No description |


## `laser_emitter` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **intensity** | float | {"intensity":1} | No description |
| **color** | vec3 (red, green, blue) | {"color":[0, 0, 0]} | No description |
| **on** | bool | {"on":true} | No description |
| **start_distance** | float | {"start_distance":0} | No description |


## `laser_line` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **intensity** | float | {"intensity":1} | No description |
| **media_density** | float | {"media_density":0.6} | No description |
| **on** | bool | {"on":true} | No description |
| **relative** | bool | {"relative":true} | No description |
| **radius** | float | {"radius":0.003} | No description |


## `laser_sensor` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **threshold** | vec3 | {"threshold":[0.5, 0.5, 0.5]} | No description |


## `light` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **intensity** | float | {"intensity":0} | No description |
| **illuminance** | float | {"illuminance":0} | No description |
| **spotAngle** | float (degrees) | {"spotAngle":0} | No description |
| **tint** | vec3 (red, green, blue) | {"tint":[1, 1, 1]} | No description |
| **gel** | string | {"gel":""} | No description |
| **on** | bool | {"on":true} | No description |
| **shadowMapSize** | uint32_t | {"shadowMapSize":9} | No description |
| **shadowMapClip** | vec2 | {"shadowMapClip":[0.1, 256]} | No description |


## `light_sensor` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **position** | vec3 | {"position":[0, 0, 0]} | No description |
| **direction** | vec3 | {"direction":[0, 0, -1]} | No description |
| **color_value** | vec3 | {"color_value":[0, 0, 0]} | No description |


## `optic` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **pass_tint** | vec3 (red, green, blue) | {"pass_tint":[0, 0, 0]} | No description |
| **reflect_tint** | vec3 (red, green, blue) | {"reflect_tint":[1, 1, 1]} | No description |
| **single_direction** | bool | {"single_direction":false} | No description |


## `physics_joints` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **physics_joints** | vector<`PhysicsJoint`> | [] | No description |


## `physics_query` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


## `scene_connection` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


## `screen` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **target** | string | {"target":""} | No description |
| **luminance** | vec3 | {"luminance":[1, 1, 1]} | No description |


## `sound` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **Sounds** | vector<`Sound`> | [] | No description |


## `trigger_area` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **trigger_area** | enum "Box", or "Sphere" | "Box" | No description |


## `trigger_group` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **trigger_group** | enum "Player", "Object", or "Magnetic" | "Player" | No description |


## `view` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **offset** | ivec2 | {"offset":[0, 0]} | No description |
| **extents** | ivec2 | {"extents":[0, 0]} | No description |
| **fov** | float (degrees) | {"fov":0} | No description |
| **clip** | vec2 | {"clip":[0.1, 256]} | No description |
| **visibilityMask** | flags "DirectCamera|DirectEye|Transparent|LightingShadow|LightingVoxel|Optics|OutlineSelection" | {"visibilityMask":""} | No description |


## `voxel_area` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **extents** | ivec3 | {"extents":[128, 128, 128]} | No description |


## `xr_view` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **xr_view** | enum "Left", or "Right" | "Left" | No description |


## `event_input` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


## `event_bindings` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


## `signal_output` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


## `signal_bindings` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


## `script` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **Scripts** | vector<`ScriptInstance`> | [] | No description |


