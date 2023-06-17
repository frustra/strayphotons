
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

A physics actor's type determines how it behaves in the world. The type should match the intended usage of an object. Dynamic actor's positions are taken over by the physics system, but scripts may still control these actors with physics joints or force-based constraints.

This is an **enum** type, and can be one of the following case-sensitive values:
- "**Static**" - The physics actor will not move. Used for walls, floors, and other static objects.
- "**Dynamic**" - The physics actor has a mass and is affected by gravity.
- "**Kinematic**" - The physics actor has infinite mass and is controlled by script or animation.
- "**SubActor**" - The shapes defined on this virtual physics actor are added to the parent physics actor instead.

</div>

<div class="type_definition">

### `PhysicsGroup` Type

An actor's physics group determines both what it will collide with in the physics simulation, and which physics queries it is visible to.

This is an **enum** type, and can be one of the following case-sensitive values:
- "**NoClip**" - Actors in this collision group will not collide with anything.
- "**World**" - This is the default collision group. All actors in this group will collide with eachother.
- "**Interactive**" - This group behaves like `World` but allows behavior to be customized for movable objects.
- "**HeldObject**" - Held objects do not collide with the player, but will collide with other held objects and the rest of the world.
- "**Player**" - This group is for the player's body, which collides with the world, but not other objects in any of the player groups.
- "**PlayerLeftHand**" - The player's left hand collides with the right hand, but not itself or the player's body.
- "**PlayerRightHand**" - The player's right hand collides with the left hand, but not itself or the player's body.
- "**UserInterface**" - This collision group is for popup UI elements that will only collide with the player's hands.

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

