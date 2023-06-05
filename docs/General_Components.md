## Common Types

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



## `name` Component

This component is required on all entities to allow for name-based references.
If no name is provided upon entity creation, an auto-generated name will be filled in.

Names are in the form:  
`"<scene_name>:<entity_name>"`

An example could be `"hello_world:platform"`.

By leaving out the scene qualifier, names can also be defined relative to their entity scope.
Inside the scene definiton the entity scope might be `"hello_world:"`,
meaning both `"hello_world:platform"` and `"platform"` would reference the same entity.

Relative names specified in a template take the form:  
`"<scene_name>:<root_name>.<relative_name>"`

The special `"scoperoot"` alias can also be used inside a template to reference the parent entity.



## `transform` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | Specifies the entity's position in 3D space. The +X direction represents Right, +Y represents Up, and -Z represents Forward. |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | Specifies the entity's orientation in 3D space. Multiple rotations can be combined by specifying an array of rotations: `[[90, 1, 0, 0], [-90, 0, 1, 0]]` is equivalent to `[120, 1, -1, -1]`. The rotation axis is automatically normalized. |
| **scale** | vec3 | [1, 1, 1] | Specifies the entity's size along each axis. A value of `[1, 1, 1]` leaves the size unchanged. If the scale is the same on all axes, a single scalar can be specified like `"scale": 0.5` |
| **parent** | `EntityRef` | "" | Specifies a parent entity that this transform is relative to. If empty, the transform is relative to the scene root. |

Multiple entities with transforms can be linked together to create a tree of entities that all move together (i.e. a transform tree).

Transforms are combined in the following way:  
`scale -> rotate -> translate ( -> parent transform)`

Note: When combining multiple transformations together with scaling factors,
behavior is undefined if the combinations introduce skew. (The scale should be axis-aligned to the model)


**See Also:**
`EntityRef`


## `transform_snapshot` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | Specifies the entity's position in 3D space. The +X direction represents Right, +Y represents Up, and -Z represents Forward. |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | Specifies the entity's orientation in 3D space. Multiple rotations can be combined by specifying an array of rotations: `[[90, 1, 0, 0], [-90, 0, 1, 0]]` is equivalent to `[120, 1, -1, -1]`. The rotation axis is automatically normalized. |
| **scale** | vec3 | [1, 1, 1] | Specifies the entity's size along each axis. A value of `[1, 1, 1]` leaves the size unchanged. If the scale is the same on all axes, a single scalar can be specified like `"scale": 0.5` |

Transform snapshots should not be set directly.
They are automatically generated for all entities with a `transform` component, and updated by the physics system.

This component stores a flattened version of an entity's transform tree. 
This represents and an entity's absolute position, orientation, and scale in the world relative to the origin.

Transform snapshots are used by the render thread for drawing entities in a physics-synchronized state,
while allowing multiple threads to independantly update entity transforms.
Snapshots are also useful for reading in scripts to reduce matrix multiplication costs and for similar sychronization benefits.



## `event_bindings` Component
The `event_bindings` component has type: map&lt;string, vector&lt;`EventBinding`&gt;&gt;

### `EventBinding` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **outputs** | vector&lt;`EventDest`&gt; | [] | No description |
| **filter** | optional&lt;`SignalExpression`&gt; | null | No description |
| **modify** | vector&lt;`SignalExpression`&gt; | [] | No description |
| **set_value** | optional&lt;`EventData`&gt; | null | No description |

### `EventData` Type
Stores a variety of possible data types for sending in events (JSON supported values are: bool, double, vec2, vec3, vec4, and string).

### `EventDest` Type
An event destination in the form of a string: "target_entity/event/input"

**See Also:**
`SignalExpression`


## `event_input` Component
The `event_input` component has no public fields


## `scene_connection` Component
The `scene_connection` component has type: map&lt;string, vector&lt;`SignalExpression`&gt;&gt;

**See Also:**
`SignalExpression`


## `script` Component
The `script` component has type: vector&lt;`ScriptInstance`&gt;

### `ScriptInstance` Type

Script instances contain a script definition (referenced by name), and a list of parameters as input for the script state.
Scripts can have 2 types: 
- "prefab": Prefab scripts such as "template" will run during scene load.
- "onTick": OnTick scripts will run during in the GameLogic thread during it's frame.
            OnTick scripts starting with "physics_" will run in the Physics thread just before simulation.
            Some OnTick scripts may also define event filters to only run when events are received.



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


