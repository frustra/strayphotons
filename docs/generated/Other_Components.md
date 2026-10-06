
<div class="component_definition">

## `light_cast` Component

A component for tracking entities receiving light from this entity

| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **visible_entities** | map&lt;[Entity](#Entity-type), float&gt; | {} | The list of entities paired with the amount of light they are receiving |

<div class="type_definition">

### `Entity` Type

</div>

</div>


<div class="component_definition">

## `voxel_data` Component

Storage for voxel data that can be turned into a mesh for use as a Renderable

| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **algorithm** | string | "" | No description |
| **extents** | uvec3 | [32, 32, 32] | No description |
| **seed** | uint32 | 0 | No description |
| **data** | vector&lt;uint8&gt; | [] | No description |

</div>

