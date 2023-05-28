## `SceneProperties` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **root_transform** | `Transform` | {} | No description |
| **gravity_transform** | `Transform` | {} | No description |
| **gravity** | vec3 | [0, -9.81, 0] | No description |


## `transform_snapshot` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | Specifies the entity's position in 3D space. The +X direction represents Right, +Y represents Up, and -Z represents Forward. |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | Specifies the entity's orientation in 3D space. Multiple rotations can be automatically combined by specifying an array of rotations: `[[90, 1, 0, 0], [-90, 0, 1, 0]]` is equivalent to `[120, 1, -1, -1]` |
| **scale** | vec3 | [1, 1, 1] | Specifies the entity's size along each axis. A value of `[1, 1, 1]` leaves the size unchanged. If the scale is the same on all axes, a single scalar can be specified like `"scale": 0.5` |


## `transform` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | Specifies the entity's position in 3D space. The +X direction represents Right, +Y represents Up, and -Z represents Forward. |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | Specifies the entity's orientation in 3D space. Multiple rotations can be automatically combined by specifying an array of rotations: `[[90, 1, 0, 0], [-90, 0, 1, 0]]` is equivalent to `[120, 1, -1, -1]` |
| **scale** | vec3 | [1, 1, 1] | Specifies the entity's size along each axis. A value of `[1, 1, 1]` leaves the size unchanged. If the scale is the same on all axes, a single scalar can be specified like `"scale": 0.5` |
| **parent** | `EntityRef` | "" | Specifies a parent entity that this transform is relative to. If empty, the transform is relative to the scene root. |


## `renderable` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **model** | string | "" | Name of the GLTF model to display. Models are loaded from the `assets/models/` folder. |
| **mesh_index** | size_t | 0 | The index of the mesh to render from the GLTF model. Note, multi-mesh GLTF models can be automatically expanded into entities using the `gltf` prefab. |
| **visibility** | flags "DirectCamera\|DirectEye\|Transparent\|LightingShadow\|LightingVoxel\|Optics\|OutlineSelection" | "DirectCamera\|DirectEye\|LightingShadow\|LightingVoxel" | Visibility mask for different render passes. |
| **emissive** | float | 0 | Emissive multiplier to turn this model into a light source |
| **color_override** | vec4 (red, green, blue, alpha) | [-1, -1, -1, -1] | Override the mesh's texture to a flat RGBA color. Values are in the range 0.0 to 1.0. -1 means the original color is used. |
| **metallic_roughness_override** | vec2 | [-1, -1] | Override the mesh's metallic and roughness material properties. Values are in the range 0.0 to 1.0. -1 means the original material is used. |


## `physics` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **shapes** | vector&lt;`PhysicsShape`&gt; | [] | A list of individual shapes and models that combine to form the actor's overall collision shape. |
| **group** | enum "NoClip", "World", "Interactive", "HeldObject", "Player", "PlayerLeftHand", "PlayerRightHand", or "UserInterface" | "World" | The collision group that this actor belongs to. |
| **type** | enum "Static", "Dynamic", "Kinematic", or "SubActor" | "Dynamic" | "Dynamic" objects are affected by gravity, while Kinematic objects have an infinite mass and are only movable by game logic. "Static" objects are meant to be immovable and will not push objects if moved. The "SubActor" type adds this entity's shapes to the **parent_actor** entity instead of creating a new physics actor. |
| **parent_actor** | `EntityRef` | "" | Only used for "SubActor" type. If empty, the parent actor is determined by the `transform` parent. |
| **mass** | float | 0 | The weight of the physics actor in Kilograms (kg). If set to 0, **density** is used instead. Only used for "Dynamic" objects. |
| **density** | float | 1000 | The density of the physics actor in Kilograms per Cubic Meter (kg/m^3). Overriden if **mass** != 0.Only used for "Dynamic" objects. |
| **angular_damping** | float | 0.05 | Resistance to changes in rotational velocity. Affects how quickly the entity will stop spinning. (>= 0.0) |
| **linear_damping** | float | 0 | Resistance to changes in linear velocity. Affects how quickly the entity will stop moving. (>= 0.0) |
| **contact_report_force** | float | -1 | The minimum collision force required to trigger a contact event. Force-based contact events are enabled if this value is >= 0.0 |
| **constant_force** | vec3 | [0, 0, 0] | A vector defining a constant force (in Newtons, N) that should be applied to the actor. The force vector is applied relative to the actor at its center of mass. |


## `animation` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **states** | vector&lt;`AnimationState`&gt; | [] | No description |
| **interpolation** | enum "Step", "Linear", or "Cubic" | "Linear" | No description |
| **cubic_tension** | float | 0.5 | No description |


## `audio` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | vector&lt;`Sound`&gt; | [] | The `audio` component is serialized directly as an array |


## `character_controller` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **head** | `EntityRef` | "" | No description |


## `gui` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **window_name** | string | "" | No description |
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
|   | vector&lt;`PhysicsJoint`&gt; | [] | The `physics_joints` component is serialized directly as an array |


## `physics_query` Component
This component has no public fields


## `scene_connection` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | map&lt;string, vector&lt;`SignalExpression`&gt;&gt; | {} | The `scene_connection` component is serialized directly as a map |


## `screen` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **target** | string | "" | No description |
| **luminance** | vec3 | [1, 1, 1] | No description |


## `trigger_area` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | enum "Box", or "Sphere" | "Box" | The `trigger_area` component is serialized directly as an enum string |


## `trigger_group` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | enum "Player", "Object", or "Magnetic" | "Player" | The `trigger_group` component is serialized directly as an enum string |


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
|   | enum "Left", or "Right" | "Left" | The `xr_view` component is serialized directly as an enum string |


## `event_input` Component
This component has no public fields


## `event_bindings` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | map&lt;string, vector&lt;`EventBinding`&gt;&gt; | {} | The `event_bindings` component is serialized directly as a map |


## `signal_output` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | map&lt;string, double&gt; | {} | The `signal_output` component is serialized directly as a map |


## `signal_bindings` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | map&lt;string, `SignalExpression`&gt; | {} | The `signal_bindings` component is serialized directly as a map |


## `script` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
|   | vector&lt;`ScriptInstance`&gt; | [] | The `script` component is serialized directly as an array |


