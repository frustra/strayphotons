## `scene_properties` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **root_transform** | struct ecs::Transform | null | No description |
| **gravity_transform** | struct ecs::Transform | {"gravity_transform":{"rotate":[0, 0, 0, 1],"scale":[1, 1, 1],"translate":[0, 0, 0]}} | No description |
| **gravity** | struct glm::vec<3,float,0> | {"gravity":[0, -9.81, 0]} | No description |


## `transform_snapshot` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


## `transform` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **translate** | struct glm::vec<3,float,0> | null | No description |
| **scale** | struct glm::vec<3,float,0> | null | No description |
| **parent** | class ecs::EntityRef | {"parent":""} | No description |


## `renderable` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **model** | class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > | {"model":""} | No description |
| **mesh_index** | unsigned __int64 | {"mesh_index":0} | No description |
| **visibility** | enum ecs::VisibilityMask | {"visibility":"DirectCamera|DirectEye|LightingShadow|LightingVoxel"} | No description |
| **emissive** | float | {"emissive":0} | No description |
| **color_override** | struct sp::color_alpha_t | {"color_override":[-1, -1, -1, -1]} | No description |
| **metallic_roughness_override** | struct glm::vec<2,float,0> | {"metallic_roughness_override":[-1, -1]} | No description |


## `physics` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **shapes** | class std::vector<struct ecs::PhysicsShape,class std::allocator<struct ecs::PhysicsShape> > | {"shapes":[]} | No description |
| **group** | enum ecs::PhysicsGroup | {"group":"World"} | No description |
| **type** | enum ecs::PhysicsActorType | {"type":"Dynamic"} | No description |
| **parent_actor** | class ecs::EntityRef | {"parent_actor":""} | No description |
| **mass** | float | {"mass":0} | No description |
| **density** | float | {"density":1000} | No description |
| **angular_damping** | float | {"angular_damping":0.05} | No description |
| **linear_damping** | float | {"linear_damping":0} | No description |
| **contact_report_force** | float | {"contact_report_force":-1} | No description |
| **force** | struct glm::vec<3,float,0> | {"force":[0, 0, 0]} | No description |


## `animation` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **states** | class std::vector<struct ecs::AnimationState,class std::allocator<struct ecs::AnimationState> > | {"states":[]} | No description |
| **interpolation** | enum ecs::InterpolationMode | {"interpolation":"Linear"} | No description |
| **cubic_tension** | float | {"cubic_tension":0.5} | No description |


## `character_controller` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **head** | class ecs::EntityRef | {"head":""} | No description |


## `gui` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **windowName** | class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > | {"windowName":""} | No description |
| **target** | enum ecs::GuiTarget | {"target":"World"} | No description |


## `laser_emitter` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **intensity** | float | {"intensity":1} | No description |
| **color** | struct sp::color_t | {"color":[0, 0, 0]} | No description |
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
| **threshold** | struct glm::vec<3,float,0> | {"threshold":[0.5, 0.5, 0.5]} | No description |


## `light` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **intensity** | float | {"intensity":0} | No description |
| **illuminance** | float | {"illuminance":0} | No description |
| **spotAngle** | class sp::angle_t | {"spotAngle":0} | No description |
| **tint** | struct sp::color_t | {"tint":[1, 1, 1]} | No description |
| **gel** | class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > | {"gel":""} | No description |
| **on** | bool | {"on":true} | No description |
| **shadowMapSize** | unsigned int | {"shadowMapSize":9} | No description |
| **shadowMapClip** | struct glm::vec<2,float,0> | {"shadowMapClip":[0.1, 256]} | No description |


## `light_sensor` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **position** | struct glm::vec<3,float,0> | {"position":[0, 0, 0]} | No description |
| **direction** | struct glm::vec<3,float,0> | {"direction":[0, 0, -1]} | No description |
| **color_value** | struct glm::vec<3,float,0> | {"color_value":[0, 0, 0]} | No description |


## `optic` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **pass_tint** | struct sp::color_t | {"pass_tint":[0, 0, 0]} | No description |
| **reflect_tint** | struct sp::color_t | {"reflect_tint":[1, 1, 1]} | No description |
| **single_direction** | bool | {"single_direction":false} | No description |


## `physics_joints` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


## `physics_query` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


## `scene_connection` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


## `screen` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **target** | class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > | {"target":""} | No description |
| **luminance** | struct glm::vec<3,float,0> | {"luminance":[1, 1, 1]} | No description |


## `sound` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


## `trigger_area` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


## `trigger_group` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


## `view` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **offset** | struct glm::vec<2,int,0> | {"offset":[0, 0]} | No description |
| **extents** | struct glm::vec<2,int,0> | {"extents":[0, 0]} | No description |
| **fov** | class sp::angle_t | {"fov":0} | No description |
| **clip** | struct glm::vec<2,float,0> | {"clip":[0.1, 256]} | No description |
| **visibilityMask** | enum ecs::VisibilityMask | {"visibilityMask":""} | No description |


## `voxel_area` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|
| **extents** | struct glm::vec<3,int,0> | {"extents":[128, 128, 128]} | No description |


## `xr_view` Component
| Field Name | Type | Default Value | Description |
|------------|------|---------------|-------------|


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


