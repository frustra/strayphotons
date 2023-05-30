## `SceneProperties` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **root_transform** | `Transform` | {} | No description |
| **gravity_transform** | `Transform` | {} | No description |
| **gravity** | vec3 | [0, -9.81, 0] | No description |

### `Transform` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | Specifies the entity's position in 3D space. The +X direction represents Right, +Y represents Up, and -Z represents Forward. |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | Specifies the entity's orientation in 3D space. Multiple rotations can be automatically combined by specifying an array of rotations: `[[90, 1, 0, 0], [-90, 0, 1, 0]]` is equivalent to `[120, 1, -1, -1]` |
| **scale** | vec3 | [1, 1, 1] | Specifies the entity's size along each axis. A value of `[1, 1, 1]` leaves the size unchanged. If the scale is the same on all axes, a single scalar can be specified like `"scale": 0.5` |


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

**See Also:**
`EntityRef`


## `renderable` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **model** | string | "" | Name of the GLTF model to display. Models are loaded from the `assets/models/` folder. |
| **mesh_index** | size_t | 0 | The index of the mesh to render from the GLTF model. Note, multi-mesh GLTF models can be automatically expanded into entities using the `gltf` prefab. |
| **visibility** | enum flags `VisibilityMask` | "DirectCamera\|DirectEye\|LightingShadow\|LightingVoxel" | Visibility mask for different render passes. |
| **emissive** | float | 0 | Emissive multiplier to turn this model into a light source |
| **color_override** | vec4 (red, green, blue, alpha) | [-1, -1, -1, -1] | Override the mesh's texture to a flat RGBA color. Values are in the range 0.0 to 1.0. -1 means the original color is used. |
| **metallic_roughness_override** | vec2 | [-1, -1] | Override the mesh's metallic and roughness material properties. Values are in the range 0.0 to 1.0. -1 means the original material is used. |

### `VisibilityMask` Type
This is a flags enum type. Multiple flags can be combined using the '|' character (e.g. `"One|Two"` with no whitespace).
Note: Enum string names are case-sensitive.
| Enum Value |
|------------|
| "DirectCamera" |
| "DirectEye" |
| "Transparent" |
| "LightingShadow" |
| "LightingVoxel" |
| "Optics" |
| "OutlineSelection" |


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

**See Also:**
`EntityRef`
`PhysicsShape`


## `animation` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **states** | vector&lt;`AnimationState`&gt; | [] | No description |
| **interpolation** | enum `InterpolationMode` | "Linear" | No description |
| **cubic_tension** | float | 0.5 | No description |

### `AnimationState` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **delay** | double | 0 | The time it takes to move to this state from any other state (in seconds) |
| **translate** | vec3 | [0, 0, 0] | A new position to override this entity's `transform` |
| **scale** | vec3 | [0, 0, 0] | A new scale to override this entity's `transform`. A scale of 0 will leave the transform unchanged. |
| **translate_tangent** | vec3 | [0, 0, 0] | Cubic interpolation tangent vector for **translate** (represents speed) |
| **scale_tangent** | vec3 | [0, 0, 0] | Cubic interpolation tangent vector for **scale** (represents rate of scaling) |

### `InterpolationMode` Type
Note: Enum string names are case-sensitive.
| Enum Value |
|------------|
| "Step" |
| "Linear" |
| "Cubic" |


## `audio` Component
The `audio` component has type: vector&lt;`Sound`&gt;

### `Sound` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **type** | enum `SoundType` | "Object" | No description |
| **file** | string | "" | No description |
| **loop** | bool | false | No description |
| **play_on_load** | bool | false | No description |
| **volume** | float | 1 | No description |

### `SoundType` Type
Note: Enum string names are case-sensitive.
| Enum Value |
|------------|
| "Object" |
| "Stereo" |
| "Ambisonic" |


## `character_controller` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **head** | `EntityRef` | "" | No description |

**See Also:**
`EntityRef`


## `gui` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **window_name** | string | "" | No description |
| **target** | enum `GuiTarget` | "World" | No description |

### `GuiTarget` Type
Note: Enum string names are case-sensitive.
| Enum Value |
|------------|
| "None" |
| "World" |
| "Debug" |


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
| **intensity** | float | 0 | The brightness of the light measured in candela (lumens per solid angle). This value is ignored if **illuminance** != 0. |
| **illuminance** | float | 0 | The brightness of the light measured in lux (lumens per square meter). This has the behavior of making the light's brightness independant of distance from the light. Overrides **intensity** field. |
| **spot_angle** | float (degrees) | 0 | The angle from the middle to the edge of the light's field of view cone. This will be half the light's overall field of view. |
| **tint** | vec3 (red, green, blue) | [1, 1, 1] | The color of light to be emitted |
| **gel** | string | "" | A lighting gel (or light filter) texture to be applied to this light. Asset textures can be referenced with the format "asset:<asset_path>.png", or render graph outputs can be referenced with the format "graph:<graph_output_name>" |
| **on** | bool | true | A flag to turn this light on and off without changing the light's values. |
| **shadow_map_size** | uint32 | 9 | All shadow maps are square powers of 2 in resolution. Each light's shadow map resolution is defined as `2^shadow_map_size`. For example, a map size of 10 would result in a 1024x1024 shadow map resolution. |
| **shadow_map_clip** | vec2 | [0.1, 256] | The near and far clipping plane distances for this light. For example, with a clip value of `[1, 10]`, light won't start hitting objects until the near plane, 1 meter from the light. The light will then cast shadows for the next 9 meters until the far plane, 10 meters from the light. |


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


## `scene_connection` Component
The `scene_connection` component has type: map&lt;string, vector&lt;`SignalExpression`&gt;&gt;

**See Also:**
`SignalExpression`


## `screen` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **target** | string | "" | No description |
| **luminance** | vec3 | [1, 1, 1] | No description |


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


## `view` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **offset** | ivec2 | [0, 0] | No description |
| **extents** | ivec2 | [0, 0] | No description |
| **fov** | float (degrees) | 0 | No description |
| **clip** | vec2 | [0.1, 256] | No description |
| **visibility_mask** | enum flags `VisibilityMask` | "" | No description |

### `VisibilityMask` Type
This is a flags enum type. Multiple flags can be combined using the '|' character (e.g. `"One|Two"` with no whitespace).
Note: Enum string names are case-sensitive.
| Enum Value |
|------------|
| "DirectCamera" |
| "DirectEye" |
| "Transparent" |
| "LightingShadow" |
| "LightingVoxel" |
| "Optics" |
| "OutlineSelection" |


## `voxel_area` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **extents** | ivec3 | [128, 128, 128] | No description |


## `xr_view` Component
The `xr_view` component has type: enum `XrEye`

### `XrEye` Type
Note: Enum string names are case-sensitive.
| Enum Value |
|------------|
| "Left" |
| "Right" |


## `event_input` Component
The `event_input` component has no public fields


## `event_bindings` Component
The `event_bindings` component has type: map&lt;string, vector&lt;`EventBinding`&gt;&gt;

### `EventBinding` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **outputs** | vector&lt;`EventDest`&gt; | [] | No description |
| **filter** | optional&lt;`SignalExpression`&gt; | {} | No description |
| **modify** | vector&lt;`SignalExpression`&gt; | [] | No description |
| **set_value** | optional&lt;`EventData`&gt; | {} | No description |

**See Also:**
`EventData`
`EventDest`
`SignalExpression`


## `signal_output` Component
The `signal_output` component has type: map&lt;string, double&gt;


## `signal_bindings` Component
The `signal_bindings` component has type: map&lt;string, `SignalExpression`&gt;

**See Also:**
`SignalExpression`


## `script` Component
The `script` component has type: vector&lt;`ScriptInstance`&gt;

**See Also:**
`ScriptInstance`


