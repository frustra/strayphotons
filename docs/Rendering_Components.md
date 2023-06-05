## `renderable` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **model** | string | "" | Name of the GLTF model to display. Models are loaded from the `assets/models/` folder. |
| **mesh_index** | size_t | 0 | The index of the mesh to render from the GLTF model. Note, multi-mesh GLTF models can be automatically expanded into entities using the `gltf` prefab. |
| **visibility** | enum flags `VisibilityMask` | "DirectCamera\|DirectEye\|LightingShadow\|LightingVoxel" | Visibility mask for different render passes. |
| **emissive** | float | 0 | Emissive multiplier to turn this model into a light source |
| **color_override** | vec4 (red, green, blue, alpha) | [-1, -1, -1, -1] | Override the mesh's texture to a flat RGBA color. Values are in the range 0.0 to 1.0. -1 means the original color is used. |
| **metallic_roughness_override** | vec2 | [-1, -1] | Override the mesh's metallic and roughness material properties. Values are in the range 0.0 to 1.0. -1 means the original material is used. |

Models are loaded from the `assets/models/` folder. `.glb` and `.gltf` are supported,
and models can be loaded from either `assets/models/<model_name>.gltf` or `assets/models/<model_name>/model_name.gltf`.

Note for GLTF models with multiple meshes:  
It is usually preferred to load the model using the `gltf` prefab script to automatically generate the correct transform tree and entity structure.

See the definition here: https://github.com/frustra/strayphotons/blob/master/src/scripts/prefabs/GltfPrefab.cc


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


## `laser_line` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **intensity** | float | 1 | No description |
| **media_density** | float | 0.6 | No description |
| **on** | bool | true | No description |
| **relative** | bool | true | No description |
| **radius** | float | 0.003 | No description |


## `light_sensor` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **position** | vec3 | [0, 0, 0] | No description |
| **direction** | vec3 | [0, 0, -1] | No description |
| **color_value** | vec3 | [0, 0, 0] | No description |


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


## `optic` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **pass_tint** | vec3 (red, green, blue) | [0, 0, 0] | No description |
| **reflect_tint** | vec3 (red, green, blue) | [1, 1, 1] | No description |
| **single_direction** | bool | false | No description |


## `screen` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **target** | string | "" | No description |
| **luminance** | vec3 | [1, 1, 1] | No description |


## `view` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **offset** | ivec2 | [0, 0] | No description |
| **extents** | ivec2 | [0, 0] | No description |
| **fov** | float (degrees) | 0 | No description |
| **clip** | vec2 | [0.1, 256] | No description |
| **visibility_mask** | enum flags `VisibilityMask` | "" | No description |

**See Also:**
`VisibilityMask`


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


