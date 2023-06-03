## `name` Component
The `name` component has type: string


## `transform` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | Specifies the entity's position in 3D space. The +X direction represents Right, +Y represents Up, and -Z represents Forward. |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | Specifies the entity's orientation in 3D space. Multiple rotations can be automatically combined by specifying an array of rotations: `[[90, 1, 0, 0], [-90, 0, 1, 0]]` is equivalent to `[120, 1, -1, -1]` |
| **scale** | vec3 | [1, 1, 1] | Specifies the entity's size along each axis. A value of `[1, 1, 1]` leaves the size unchanged. If the scale is the same on all axes, a single scalar can be specified like `"scale": 0.5` |
| **parent** | `EntityRef` | "" | Specifies a parent entity that this transform is relative to. If empty, the transform is relative to the scene root. |

**See Also:**
`EntityRef`


## `transform_snapshot` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | Specifies the entity's position in 3D space. The +X direction represents Right, +Y represents Up, and -Z represents Forward. |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | Specifies the entity's orientation in 3D space. Multiple rotations can be automatically combined by specifying an array of rotations: `[[90, 1, 0, 0], [-90, 0, 1, 0]]` is equivalent to `[120, 1, -1, -1]` |
| **scale** | vec3 | [1, 1, 1] | Specifies the entity's size along each axis. A value of `[1, 1, 1]` leaves the size unchanged. If the scale is the same on all axes, a single scalar can be specified like `"scale": 0.5` |


## `event_bindings` Component
The `event_bindings` component has type: map&lt;string, vector&lt;`EventBinding`&gt;&gt;

### `EventBinding` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **outputs** | vector&lt;`EventDest`&gt; | [] | No description |
| **filter** | optional&lt;`SignalExpression`&gt; | null | No description |
| **modify** | vector&lt;`SignalExpression`&gt; | [] | No description |
| **set_value** | optional&lt;`EventData`&gt; | null | No description |

**See Also:**
`EventData`
`EventDest`
`SignalExpression`


## `event_input` Component
The `event_input` component has no public fields


## `scene_connection` Component
The `scene_connection` component has type: map&lt;string, vector&lt;`SignalExpression`&gt;&gt;

**See Also:**
`SignalExpression`


## `script` Component
The `script` component has type: vector&lt;`ScriptInstance`&gt;

**See Also:**
`ScriptInstance`


## `signal_bindings` Component
The `signal_bindings` component has type: map&lt;string, `SignalExpression`&gt;

**See Also:**
`SignalExpression`


## `signal_output` Component
The `signal_output` component has type: map&lt;string, double&gt;


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


