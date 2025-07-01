
<div class="component_definition">

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

| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **scene** | string (max 63 chars) | "" | No description |
| **entity** | string (max 63 chars) | "" | No description |

</div>


<div class="component_definition">

## `transform` Component

Transforms are performed in the following order:  
`scale -> rotate -> translate ( -> parent transform)`

Multiple entities with transforms can be linked together to create a tree of entities that all move together (i.e. a transform tree).

Note: When combining multiple transformations together with scaling factors,
behavior is undefined if the combinations introduce skew. (The scale should be axis-aligned to the model)

| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | Specifies the entity's position in 3D space. The +X direction represents Right, +Y represents Up, and -Z represents Forward. |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | Specifies the entity's orientation in 3D space. Multiple rotations can be combined by specifying an array of rotations: `[[90, 1, 0, 0], [-90, 0, 1, 0]]` is equivalent to `[120, 1, -1, -1]`. The rotation axis is automatically normalized. |
| **scale** | vec3 | [1, 1, 1] | Specifies the entity's size along each axis. A value of `[1, 1, 1]` leaves the size unchanged. If the scale is the same on all axes, a single scalar can be specified like `"scale": 0.5` |
| **parent** | [EntityRef](#EntityRef-type) | "" | Specifies a parent entity that this transform is relative to. If empty, the transform is relative to the scene root. |

**See Also:**
[EntityRef](#EntityRef-type)

</div>


<div class="component_definition">

## `transform_snapshot` Component

Transform snapshots should not be set directly.
They are automatically generated for all entities with a `transform` component, and updated by the physics system.

This component stores a flattened version of an entity's transform tree. 
This represents and an entity's absolute position, orientation, and scale in the world relative to the origin.

Transform snapshots are used by the render thread for drawing entities in a physics-synchronized state,
while allowing multiple threads to independantly update entity transforms.
Snapshots are also useful for reading in scripts to reduce matrix multiplication costs and for similar sychronization benefits.

| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | vec3 | [0, 0, 0] | Specifies the entity's position in 3D space. The +X direction represents Right, +Y represents Up, and -Z represents Forward. |
| **rotate** | vec4 (angle_degrees, axis_x, axis_y, axis_z) | [0, 0, 0, 1] | Specifies the entity's orientation in 3D space. Multiple rotations can be combined by specifying an array of rotations: `[[90, 1, 0, 0], [-90, 0, 1, 0]]` is equivalent to `[120, 1, -1, -1]`. The rotation axis is automatically normalized. |
| **scale** | vec3 | [1, 1, 1] | Specifies the entity's size along each axis. A value of `[1, 1, 1]` leaves the size unchanged. If the scale is the same on all axes, a single scalar can be specified like `"scale": 0.5` |

</div>


<div class="component_definition">

## `event_bindings` Component

Event bindings, along with [`signal_bindings`](#signal_bindings-component), are the main ways entites 
communicate with eachother. When an event is generated at an entity, it will first be delivered to 
any local event queues in the [`event_input` Component](#event_input-component). 
Then it will be forwarded through any matching event bindings to be delivered to other entities.

In their most basic form, event bindings will simply forward events to a new target:
```json
"event_bindings": {
    "/button1/action/press": "player:player/action/jump",
    "/trigger/object/enter": ["scene:object1/notify/react", "scene:object2/notify/react"]
}
```
For the above bindings, when the input event `/button1/action/press` is generated at the local entity, 
it will be forwarded to the `player:player` entity as the `/action/jump` event with the same event data. 
Similarly, the `/trigger/object/enter` will be forwarded to both the `scene:object1` and `scene:object2` 
entities as the `/notify/react` event, again with the original event data intact.

> [!WARNING]
> Events are forwarded recursively until a maximum binding depth is reached.  
> Bindings should not contain loops or deep binding trees, otherwise events will be dropped.

More advanced event bindings can also filter and modify event data as is passes through them. 
For example, the following binding is evaluated for each `/keyboard/key/v` input event:
```json
"event_bindings": {
    "/keyboard/key/v": {
        "outputs": "console:input/action/run_command",
        "filter": "is_focused(Game) && event",
        "set_value": "togglesignal player:player/move_noclip"
    }
}
```
Key input events from the `input:keyboard` entity have a bool data type, and are generated for both the key down 
and key up events (signified by a **true** and **false** value respectively).

The above binding specifies a filter expression that will only forward events while the `Game` focus layer is active,
and will only forward key down events (true data values).

The event data is then replaced with the string `"togglesignal player:player/move_noclip"`, and forwarded to the `console:input` entity
as the `/action/run_command` event. Upon receiving this event, the `console:input` entity will execute the provided string in the console.

The `event_bindings` component has type: map&lt;string (max 127 chars), vector&lt;[EventBinding](#EventBinding-type)&gt;&gt;

<div class="type_definition">

### `EventBinding` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **outputs** | vector&lt;[EventDest](#EventDest-type)&gt; | [] | One or more event queue targets to send events to. If only a single output is needed, it can be specified directly as: `"outputs": "target_entity/target/event/queue"` |
| **filter** | optional&lt;[SignalExpression](#SignalExpression-type)&gt; | null | If a `filter` expression is specified, only events for which the filter evaluates to true (>= 0.5) are forwarded to the output. Event data can be accessed in the filter expression via the `event` keyword (e.g. `event.y > 2`) |
| **set_value** | optional&lt;[EventData](#EventData-type)&gt; | null | If `set_value` is specified, the event's data and type will be changed to the provided value. `set_value` occurs before the `modify` expression is evaluated. |
| **modify** | vector&lt;[SignalExpression](#SignalExpression-type)&gt; | [] | One or more `modify` expressions can be specified to manipulate the contents of events as they flow through an binding. 1 expression is evaluated per 'element' of the input event type (i.e. 3 exprs for vec3, 1 expr for bool, etc...). Input event data is provided via the `event` expression keyword, and the result of each expression is then converted back to the original event type before being used as the new event data.  As an example, the following `modify` expression will scale an input vec3 event by 2 on all axes: `["event.x * 2", "event.y * 2", "event.z * 2"]`. |

</div>

<div class="type_definition">

### `EventData` Type

Stores a variety of possible data types for sending in events (JSON supported values are: **bool**, **double**, **vec2**, **vec3**, **vec4**, and **string**).

</div>

<div class="type_definition">

### `EventDest` Type

An event destination in the form of a string: `"target_entity/event/input"`

</div>

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `event_input` Component

For an entity to receive events (not just forward them), it must have an `event_input` component.  
This component stores a list of event queues that have been registered to receive events on a per-entity basis.

No configuration is stored for the `event_input` component. Scripts and other systems programmatically register 
their own event queues as needed.


</div>


<div class="component_definition">

## `scene_connection` Component

The scene connection component has 2 functions:
- Scenes can be requested to load asyncronously by providing one or more signal expression conditions.  
  Scenes will stay loaded as long as at least one of the listed expressions evaluates to **true** (>= 0.5).
- If the scene connection entity also has a [`transform` Component](#transform-component), any scene being loaded
  with a matching `scene_connection` entity will have all its entities moved so that the connection points align.

The `scene_connection` component has type: map&lt;string, vector&lt;[SignalExpression](#SignalExpression-type)&gt;&gt;

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `script` Component

The `script` component has type: vector&lt;[ScriptInstance](#ScriptInstance-type)&gt;

<div class="type_definition">

### `ScriptInstance` Type

Script instances contain a script definition (referenced by name), and a list of parameters as input for the script state.  
Scripts can have 2 types: 
- **Prefab scripts** prefixed with "prefab_" will run during scene load.
- **Runtime scripts** (or OnTick scripts) will run during in GameLogic thread during its frame.  
    Runtime scripts starting with "physics_" will run in the Physics thread just before simulation.  
    Some OnTick scripts may also internally define event filters to only run when events are received.

Script instances are defined using the following fields:
| Instance Field | Type    | Description |
|----------------|---------|-------------|
| **name**     | string  | The name of a [Default Built-in Script](Default_Scripts.md) |
| **parameters** | *any*   | A set of parameters to be given to the script. See individiaul script documentation for info. |

Here is an example of an instance definition for a "spotlight" [`template` Prefab](#template-prefab):
```json
{
    "name": "prefab_template",
    "parameters": {
        "source": "spotlight"
    }
}
```

</div>

</div>


<div class="component_definition">

## `signal_bindings` Component

A signal binding is a read-only signal who's value is determined by a [SignalExpression](#signalexpression-type). 
Signal bindings can be referenced the same as signals from the [`signal_output` component](#signal_output-component).

Signal bindings are used to forward state between entities, as well as make calculations about the state of the world.  
Each signal represents a continuous value over time. When read, a binding will have its expression evaluated atomically 
such that data is synchronized between signals and between entities.

A simple signal binding to alias some values from another entity might look like this:
```json
"signal_bindings": {
    "move_forward": "input:keyboard/key_w",
    "move_back": "input:keyboard/key_s"
    "move_left": "input:keyboard/key_a",
    "move_right": "input:keyboard/key_d",
}
```
This will map the *WASD* keys to movement signals on the local entity, decoupling the input source from the game logic.  
You can see more examples of this being used in 
[Stray Photon's Input Bindings](https://github.com/frustra/strayphotons/blob/master/assets/default_input_bindings.json).

> [!WARNING]
> Signal bindings may reference other signal bindings, which will be evaluated recursively up until a maximum depth.  
> Signal expressions should not contain self-referential loops or deep reference trees to avoid unexpected `0.0` evaluations.

> [!NOTE]
> Referencing a missing entity or missing signal will result in a value of `0.0`.  
> If a signal is defined in both the [`signal_output` component](#signal_output-component) and `signal_bindings` component 
> of the same entity, the `signal_output` will take priority and override the binding.

A more complex set of bindings could be added making use of the [SignalExpression](#signalexpression-type) syntax to 
calculate an X/Y movement, and combine it with a joystick input:
```json
"signal_bindings": {
    "move_x": "player/move_right - player/move_left + vr:controller_right/actions_main_in_movement.x",
    "move_y": "player/move_forward - player/move_back + vr:controller_right/actions_main_in_movement.y"
}
```
Depending on the source of the signals, functions like `min(1, x)` and `max(-1, x)` could be added to clamp movement speed.  
Extra multipliers could also be added to adjust joystick senstiivity, movement speed, or flip axes.

For binding state associated with a time, [`event_bindings`](#event_bindings-component) are used instead of signals.

The `signal_bindings` component has type: map&lt;string, [SignalExpression](#SignalExpression-type)&gt;

**See Also:**
[SignalExpression](#SignalExpression-type)

</div>


<div class="component_definition">

## `signal_output` Component

The `signal_output` component stores a list of mutable signal values by name.  
These values are stored as 64-bit double floating-point numbers that exist continuously over time, 
and can be sampled by any [SignalExpression](#signalexpression-type).  
Signal outputs can be written to by scripts and have their value changed in response to events in the world at runtime.

> [!NOTE]
> If a signal is defined in both the `signal_output` component and [`signal_bindings`](#signal_bindings-component) 
> of the same entity, the `signal_output` will take priority and override the binding.

Signal output components can have their initial values defined like this:
```json
"signal_output": {
    "value1": 10.0,
    "value2": 42.0,
    "other": 0.0
}
```
In the above case, setting the "other" output signal to `0.0` will override any `signal_bindings` named "other".

The `signal_output` component has type: map&lt;string, double&gt;

</div>


<div class="component_definition">

## `audio` Component

The `audio` component has type: vector&lt;[Sound](#Sound-type)&gt;

<div class="type_definition">

### `Sound` Type
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **type** | enum [SoundType](#SoundType-type) | "Object" | No description |
| **file** | string | "" | No description |
| **loop** | bool | false | No description |
| **play_on_load** | bool | false | No description |
| **volume** | float | 1 | No description |

</div>

<div class="type_definition">

### `SoundType` Type
This is an **enum** type, and can be one of the following case-sensitive values:
- "**Object**" - No description
- "**Stereo**" - No description
- "**Ambisonic**" - No description

</div>

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

