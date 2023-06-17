
<div class="component_definition">

## `gltf` Prefab
| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **model** | string | "" | No description |
| **skip_nodes** | vector&lt;string&gt; | [] | No description |
| **physics_group** | enum [PhysicsGroup](#PhysicsGroup-type) | "World" | No description |
| **render** | bool | true | No description |
| **physics** | optional&lt;enum [PhysicsActorType](#PhysicsActorType-type)&gt; | null | No description |
| **interactive** | bool | false | No description |

<div class="type_definition">

### `PhysicsActorType` Type
This is an **enum** type, and can be one of the following case-sensitive values:
- "**Static**" - No description
- "**Dynamic**" - No description
- "**Kinematic**" - No description
- "**SubActor**" - No description

</div>

<div class="type_definition">

### `PhysicsGroup` Type
This is an **enum** type, and can be one of the following case-sensitive values:
- "**NoClip**" - No description
- "**World**" - No description
- "**Interactive**" - No description
- "**HeldObject**" - No description
- "**Player**" - No description
- "**PlayerLeftHand**" - No description
- "**PlayerRightHand**" - No description
- "**UserInterface**" - No description

</div>

</div>


<div class="component_definition">

## `life_cell` Prefab
| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **board_size** | uvec2 | [32, 32] | No description |

</div>


<div class="component_definition">

## `template` Prefab
| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **source** | string | "" | No description |

</div>


<div class="component_definition">

## `tile` Prefab
| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **count** | ivec2 | [1, 1] | No description |
| **stride** | vec2 | [1, 1] | No description |
| **axes** | string | "xy" | No description |
| **surface** | string | "" | No description |
| **edge** | string | "" | No description |
| **corner** | string | "" | No description |

</div>


<div class="component_definition">

## `wall` Prefab
| Parameter Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **y_offset** | float | 0 | No description |
| **stride** | float | 1 | No description |
| **segments** | vector&lt;vec2&gt; | [] | No description |
| **segment_types** | vector&lt;string&gt; | [] | No description |

</div>

