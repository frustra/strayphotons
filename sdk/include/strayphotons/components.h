/*
 * THIS FILE IS AUTO-GENERATED -- DO NOT EDIT
 * See src/exports/codegen/gen_components.hh to modify
 */

 /*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <c_abi/Tecs.h>
#include <strayphotons/entity.h>
#include <strayphotons/export.h>
#include <strayphotons/graphics.h>

#if !defined(__cplusplus) || !defined(SP_SHARED_INTERNAL)
#ifdef __cplusplus
extern "C" {
#endif
#pragma pack(push, 1)
const uint32_t SP_TYPE_INDEX_ECS_NAME = 126;
const uint32_t SP_TYPE_INDEX_STRING_63 = 8;
typedef char string_63_t[64];
// Component: Name
typedef struct sp_ecs_name_t {
    string_63_t scene; // 64 bytes
    string_63_t entity; // 64 bytes
} sp_ecs_name_t; // 128 bytes
const uint64_t SP_NAME_INDEX = 0;
const uint64_t SP_ACCESS_NAME = 2ull << 0;
SP_EXPORT sp_ecs_name_t *sp_entity_set_name(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_name_t *sp_entity_get_name(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_name_t *sp_entity_get_const_name(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_name(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_SCENE_INFO = 127;
// Component: SceneInfo
typedef struct sp_ecs_scene_info_t {
    const uint8_t _unknown0[208];
} sp_ecs_scene_info_t; // 208 bytes
const uint64_t SP_SCENE_INFO_INDEX = 1;
const uint64_t SP_ACCESS_SCENE_INFO = 2ull << 1;
SP_EXPORT sp_ecs_scene_info_t *sp_entity_set_scene_info(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_scene_info_t *sp_entity_get_scene_info(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_scene_info_t *sp_entity_get_const_scene_info(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_scene_info(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_SCENE_PROPERTIES = 128;
const uint32_t SP_TYPE_INDEX_TRANSFORM = 4;
const uint32_t SP_TYPE_INDEX_VEC3 = 3;
typedef struct vec3_t { float v[3]; } vec3_t;
const uint32_t SP_TYPE_INDEX_MAT3 = 16;
typedef struct mat3_t { float m[3][3]; } mat3_t;
// Type: ecs::Transform
typedef struct sp_transform_t {
    mat3_t rotate; // 36 bytes
    vec3_t translate; // 12 bytes
    vec3_t scale; // 12 bytes
} sp_transform_t; // 60 bytes
const uint32_t SP_TYPE_INDEX_VOID = 0;
const uint32_t SP_TYPE_INDEX_FLOAT = 1;
const uint32_t SP_TYPE_INDEX_QUAT = 38;
typedef struct quat_t { float q[4]; } quat_t;
const uint32_t SP_TYPE_INDEX_MAT4 = 39;
typedef struct mat4_t { float m[4][4]; } mat4_t;
SP_EXPORT void sp_transform_translate(sp_transform_t *self, const vec3_t * xyz);
SP_EXPORT void sp_transform_rotate_axis(sp_transform_t *self, float radians, const vec3_t * axis);
SP_EXPORT void sp_transform_rotate(sp_transform_t *self, const quat_t * quat);
SP_EXPORT void sp_transform_scale(sp_transform_t *self, const vec3_t * xyz);
SP_EXPORT void sp_transform_set_position(sp_transform_t *self, const vec3_t * pos);
SP_EXPORT void sp_transform_set_rotation(sp_transform_t *self, const quat_t * quat);
SP_EXPORT void sp_transform_set_scale(sp_transform_t *self, const vec3_t * xyz);
SP_EXPORT const vec3_t * sp_transform_get_position(const sp_transform_t *self);
SP_EXPORT void sp_transform_get_rotation(const sp_transform_t *self, quat_t *result);
SP_EXPORT void sp_transform_get_forward(const sp_transform_t *self, vec3_t *result);
SP_EXPORT void sp_transform_get_up(const sp_transform_t *self, vec3_t *result);
SP_EXPORT const vec3_t * sp_transform_get_scale(const sp_transform_t *self);
SP_EXPORT const sp_transform_t * sp_transform_get(const sp_transform_t *self);
SP_EXPORT void sp_transform_get_inverse(const sp_transform_t *self, sp_transform_t *result);
SP_EXPORT void sp_transform_get_matrix(const sp_transform_t *self, mat4_t *result);

// Component: SceneProperties
typedef struct sp_ecs_scene_properties_t {
    sp_transform_t root_transform; // 60 bytes
    sp_transform_t gravity_transform; // 60 bytes
    vec3_t gravity; // 12 bytes
    const uint8_t _unknown132[12];
} sp_ecs_scene_properties_t; // 144 bytes
const uint64_t SP_SCENE_PROPERTIES_INDEX = 2;
const uint64_t SP_ACCESS_SCENE_PROPERTIES = 2ull << 2;
SP_EXPORT sp_ecs_scene_properties_t *sp_entity_set_scene_properties(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_scene_properties_t *sp_entity_get_scene_properties(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_scene_properties_t *sp_entity_get_const_scene_properties(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_scene_properties(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_TRANSFORM_SNAPSHOT = 129;
// Component: TransformSnapshot
typedef struct sp_ecs_transform_snapshot_t {
    sp_transform_t transform; // 60 bytes
} sp_ecs_transform_snapshot_t; // 60 bytes
const uint64_t SP_TRANSFORM_SNAPSHOT_INDEX = 3;
const uint64_t SP_ACCESS_TRANSFORM_SNAPSHOT = 2ull << 3;
SP_EXPORT sp_ecs_transform_snapshot_t *sp_entity_set_transform_snapshot(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_transform_snapshot_t *sp_entity_get_transform_snapshot(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_transform_snapshot_t *sp_entity_get_const_transform_snapshot(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_transform_snapshot(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_TRANSFORM_TREE = 130;
const uint32_t SP_TYPE_INDEX_ENTITY_REF = 17;
// Type: ecs::EntityRef
typedef struct sp_entity_ref_t {
    const uint8_t _unknown0[16];
} sp_entity_ref_t; // 16 bytes
const uint32_t SP_TYPE_INDEX_TECS_ENTITY = 26;
const uint32_t SP_TYPE_INDEX_BOOL = 19;
const uint32_t SP_TYPE_INDEX_CHAR = 20;
SP_EXPORT void sp_entity_ref_name(const sp_entity_ref_t *self, sp_ecs_name_t *result);
SP_EXPORT tecs_entity_t sp_entity_ref_get(const sp_entity_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT bool sp_entity_ref_is_valid(const sp_entity_ref_t *self);
SP_EXPORT void sp_entity_ref_empty(sp_entity_ref_t *result);
SP_EXPORT void sp_entity_ref_new(tecs_entity_t ent, sp_entity_ref_t *result);
SP_EXPORT void sp_entity_ref_copy(const sp_entity_ref_t * ref, sp_entity_ref_t *result);
SP_EXPORT void sp_entity_ref_lookup(const char * name, const sp_ecs_name_t * scope, sp_entity_ref_t *result);
SP_EXPORT void sp_entity_ref_clear(sp_entity_ref_t *self);

// Component: TransformTree
typedef struct sp_ecs_transform_tree_t {
    sp_transform_t transform; // 60 bytes
    const uint8_t _unknown60[4];
    sp_entity_ref_t parent; // 16 bytes
} sp_ecs_transform_tree_t; // 80 bytes
const uint64_t SP_TRANSFORM_TREE_INDEX = 4;
const uint64_t SP_ACCESS_TRANSFORM_TREE = 2ull << 4;
SP_EXPORT sp_ecs_transform_tree_t *sp_entity_set_transform_tree(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_transform_tree_t *sp_entity_get_transform_tree(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_transform_tree_t *sp_entity_get_const_transform_tree(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_transform_tree(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_ecs_transform_tree_move_via_root(tecs_lock_t * arg0, tecs_entity_t arg1, sp_transform_t arg2);
SP_EXPORT tecs_entity_t sp_ecs_transform_tree_get_root(tecs_lock_t * arg0, tecs_entity_t arg1);
SP_EXPORT void sp_ecs_transform_tree_get_global_transform(const sp_ecs_transform_tree_t *self, tecs_lock_t * arg0, sp_transform_t *result);
SP_EXPORT void sp_ecs_transform_tree_get_global_rotation(const sp_ecs_transform_tree_t *self, tecs_lock_t * arg0, quat_t *result);
SP_EXPORT void sp_ecs_transform_tree_get_relative_transform(const sp_ecs_transform_tree_t *self, tecs_lock_t * arg0, const tecs_entity_t * arg1, sp_transform_t *result);

const uint32_t SP_TYPE_INDEX_ECS_RENDERABLE = 131;
const uint32_t SP_TYPE_INDEX_EVENT_STRING = 10;
typedef char event_string_t[256];
const uint32_t SP_TYPE_INDEX_UINT64 = 12;
const uint32_t SP_TYPE_INDEX_VISIBILITY_MASK = 13;
// Enum: ecs::VisibilityMask
typedef enum sp_visibility_mask_t {
    SP_VISIBILITY_MASK_DIRECT_CAMERA = 1,
    SP_VISIBILITY_MASK_DIRECT_EYE = 2,
    SP_VISIBILITY_MASK_TRANSPARENT = 4,
    SP_VISIBILITY_MASK_LIGHTING_SHADOW = 8,
    SP_VISIBILITY_MASK_LIGHTING_VOXEL = 16,
    SP_VISIBILITY_MASK_OPTICS = 32,
    SP_VISIBILITY_MASK_OUTLINE_SELECTION = 64,
} sp_visibility_mask_t;
const uint32_t SP_TYPE_INDEX_COLOR_ALPHA = 14;
typedef struct sp_color_alpha_t { float rgba[4]; } sp_color_alpha_t;
const uint32_t SP_TYPE_INDEX_EVENT_NAME = 9;
typedef char event_name_t[128];
const uint32_t SP_TYPE_INDEX_VEC2 = 2;
typedef struct vec2_t { float v[2]; } vec2_t;
// Component: renderable
typedef struct sp_ecs_renderable_t {
    event_string_t model; // 256 bytes
    const uint8_t _unknown256[16];
    uint64_t mesh_index; // 8 bytes
    const uint8_t _unknown280[24];
    sp_visibility_mask_t visibility; // 4 bytes
    float emissive; // 4 bytes
    sp_color_alpha_t color_override; // 16 bytes
    event_name_t texture_override; // 128 bytes
    vec2_t metallic_roughness_override; // 8 bytes
} sp_ecs_renderable_t; // 464 bytes
const uint64_t SP_RENDERABLE_INDEX = 5;
const uint64_t SP_ACCESS_RENDERABLE = 2ull << 5;
SP_EXPORT sp_ecs_renderable_t *sp_entity_set_renderable(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_renderable_t *sp_entity_get_renderable(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_renderable_t *sp_entity_get_const_renderable(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_renderable(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_PHYSICS = 132;
const uint32_t SP_TYPE_INDEX_PHYSICS_SHAPE_VECTOR = 77;
const uint32_t SP_TYPE_INDEX_PHYSICS_SHAPE = 48;
const uint32_t SP_TYPE_INDEX_PHYSICS_MATERIAL = 47;
// Type: ecs::PhysicsMaterial
typedef struct sp_physics_material_t {
    float static_friction; // 4 bytes
    float dynamic_friction; // 4 bytes
    float restitution; // 4 bytes
} sp_physics_material_t; // 12 bytes

// Type: ecs::PhysicsShape
typedef struct sp_physics_shape_t {
    const uint8_t _unknown0[552];
    sp_transform_t transform; // 60 bytes
    sp_physics_material_t physics_material; // 12 bytes
} sp_physics_shape_t; // 624 bytes

typedef struct sp_physics_shape_vector_t {
    const uint8_t _unknown[24];
} sp_physics_shape_vector_t;
SP_EXPORT size_t sp_physics_shape_vector_get_size(const sp_physics_shape_vector_t *v);
SP_EXPORT const sp_physics_shape_t *sp_physics_shape_vector_get_const_data(const sp_physics_shape_vector_t *v);
SP_EXPORT sp_physics_shape_t *sp_physics_shape_vector_get_data(sp_physics_shape_vector_t *v);
SP_EXPORT sp_physics_shape_t *sp_physics_shape_vector_resize(sp_physics_shape_vector_t *v, size_t new_size);

const uint32_t SP_TYPE_INDEX_PHYSICS_GROUP = 99;
// Enum: ecs::PhysicsGroup
const uint16_t SP_PHYSICS_GROUP_NO_CLIP = 0;
const uint16_t SP_PHYSICS_GROUP_WORLD = 1;
const uint16_t SP_PHYSICS_GROUP_INTERACTIVE = 2;
const uint16_t SP_PHYSICS_GROUP_HELD_OBJECT = 3;
const uint16_t SP_PHYSICS_GROUP_PLAYER = 4;
const uint16_t SP_PHYSICS_GROUP_PLAYER_LEFT_HAND = 5;
const uint16_t SP_PHYSICS_GROUP_PLAYER_RIGHT_HAND = 6;
const uint16_t SP_PHYSICS_GROUP_USER_INTERFACE = 7;
typedef uint16_t sp_physics_group_t;
const uint32_t SP_TYPE_INDEX_PHYSICS_ACTOR_TYPE = 100;
// Enum: ecs::PhysicsActorType
const uint8_t SP_PHYSICS_ACTOR_TYPE_STATIC = 0;
const uint8_t SP_PHYSICS_ACTOR_TYPE_DYNAMIC = 1;
const uint8_t SP_PHYSICS_ACTOR_TYPE_KINEMATIC = 2;
const uint8_t SP_PHYSICS_ACTOR_TYPE_SUB_ACTOR = 3;
typedef uint8_t sp_physics_actor_type_t;
// Component: physics
typedef struct sp_ecs_physics_t {
    sp_physics_shape_vector_t shapes; // 24 bytes
    sp_physics_group_t group; // 2 bytes
    sp_physics_actor_type_t type; // 1 bytes
    const uint8_t _unknown27[5];
    sp_entity_ref_t parent_actor; // 16 bytes
    float mass; // 4 bytes
    float density; // 4 bytes
    float angular_damping; // 4 bytes
    float linear_damping; // 4 bytes
    float contact_report_force; // 4 bytes
    vec3_t constant_force; // 12 bytes
} sp_ecs_physics_t; // 80 bytes
const uint64_t SP_PHYSICS_INDEX = 6;
const uint64_t SP_ACCESS_PHYSICS = 2ull << 6;
SP_EXPORT sp_ecs_physics_t *sp_entity_set_physics(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_physics_t *sp_entity_get_physics(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_physics_t *sp_entity_get_const_physics(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_physics(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_ACTIVE_SCENE = 133;
const uint32_t SP_TYPE_INDEX_SCENE_REF = 61;
// Type: sp::SceneRef
typedef struct sp_scene_ref_t {
    const uint8_t _unknown0[32];
} sp_scene_ref_t; // 32 bytes

// Component: active_scene
typedef struct sp_ecs_active_scene_t {
    sp_scene_ref_t scene; // 32 bytes
} sp_ecs_active_scene_t; // 32 bytes
const uint64_t SP_ACTIVE_SCENE_INDEX = 7;
const uint64_t SP_ACCESS_ACTIVE_SCENE = 2ull << 7;
SP_EXPORT sp_ecs_active_scene_t *sp_ecs_set_active_scene(tecs_lock_t *dynLockPtr);
SP_EXPORT sp_ecs_active_scene_t *sp_ecs_get_active_scene(tecs_lock_t *dynLockPtr);
SP_EXPORT const sp_ecs_active_scene_t *sp_ecs_get_const_active_scene(tecs_lock_t *dynLockPtr);
SP_EXPORT void sp_ecs_unset_active_scene(tecs_lock_t *dynLockPtr);

const uint32_t SP_TYPE_INDEX_ECS_ANIMATION = 134;
const uint32_t SP_TYPE_INDEX_ANIMATION_STATE_VECTOR = 75;
const uint32_t SP_TYPE_INDEX_ANIMATION_STATE = 40;
const uint32_t SP_TYPE_INDEX_DOUBLE = 15;
// Type: ecs::AnimationState
typedef struct sp_animation_state_t {
    double delay; // 8 bytes
    vec3_t translate; // 12 bytes
    vec3_t scale; // 12 bytes
    vec3_t translate_tangent; // 12 bytes
    vec3_t scale_tangent; // 12 bytes
} sp_animation_state_t; // 56 bytes

typedef struct sp_animation_state_vector_t {
    const uint8_t _unknown[24];
} sp_animation_state_vector_t;
SP_EXPORT size_t sp_animation_state_vector_get_size(const sp_animation_state_vector_t *v);
SP_EXPORT const sp_animation_state_t *sp_animation_state_vector_get_const_data(const sp_animation_state_vector_t *v);
SP_EXPORT sp_animation_state_t *sp_animation_state_vector_get_data(sp_animation_state_vector_t *v);
SP_EXPORT sp_animation_state_t *sp_animation_state_vector_resize(sp_animation_state_vector_t *v, size_t new_size);

const uint32_t SP_TYPE_INDEX_INTERPOLATION_MODE = 98;
// Enum: ecs::InterpolationMode
typedef enum sp_interpolation_mode_t {
    SP_INTERPOLATION_MODE_STEP = 0,
    SP_INTERPOLATION_MODE_LINEAR = 1,
    SP_INTERPOLATION_MODE_CUBIC = 2,
} sp_interpolation_mode_t;
// Component: animation
typedef struct sp_ecs_animation_t {
    sp_animation_state_vector_t states; // 24 bytes
    sp_interpolation_mode_t interpolation; // 4 bytes
    float cubic_tension; // 4 bytes
} sp_ecs_animation_t; // 32 bytes
const uint64_t SP_ANIMATION_INDEX = 8;
const uint64_t SP_ACCESS_ANIMATION = 2ull << 8;
SP_EXPORT sp_ecs_animation_t *sp_entity_set_animation(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_animation_t *sp_entity_get_animation(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_animation_t *sp_entity_get_const_animation(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_animation(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_AUDIO = 135;
const uint32_t SP_TYPE_INDEX_SOUND_VECTOR = 79;
const uint32_t SP_TYPE_INDEX_SOUND = 55;
const uint32_t SP_TYPE_INDEX_SOUND_TYPE = 104;
// Enum: ecs::SoundType
typedef enum sp_sound_type_t {
    SP_SOUND_TYPE_OBJECT = 0,
    SP_SOUND_TYPE_STEREO = 1,
    SP_SOUND_TYPE_AMBISONIC = 2,
} sp_sound_type_t;
// Type: ecs::Sound
typedef struct sp_sound_t {
    sp_sound_type_t type; // 4 bytes
    event_string_t file; // 256 bytes
    const uint8_t _unknown260[20];
    bool loop; // 1 bytes
    bool play_on_load; // 1 bytes
    const uint8_t _unknown282[2];
    float volume; // 4 bytes
} sp_sound_t; // 288 bytes

typedef struct sp_sound_vector_t {
    const uint8_t _unknown[24];
} sp_sound_vector_t;
SP_EXPORT size_t sp_sound_vector_get_size(const sp_sound_vector_t *v);
SP_EXPORT const sp_sound_t *sp_sound_vector_get_const_data(const sp_sound_vector_t *v);
SP_EXPORT sp_sound_t *sp_sound_vector_get_data(sp_sound_vector_t *v);
SP_EXPORT sp_sound_t *sp_sound_vector_resize(sp_sound_vector_t *v, size_t new_size);

// Component: audio
typedef struct sp_ecs_audio_t {
    sp_sound_vector_t sounds; // 24 bytes
    const uint8_t _unknown24[24];
} sp_ecs_audio_t; // 48 bytes
const uint64_t SP_AUDIO_INDEX = 9;
const uint64_t SP_ACCESS_AUDIO = 2ull << 9;
SP_EXPORT sp_ecs_audio_t *sp_entity_set_audio(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_audio_t *sp_entity_get_audio(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_audio_t *sp_entity_get_const_audio(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_audio(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_CHARACTER_CONTROLLER = 136;
// Component: character_controller
typedef struct sp_ecs_character_controller_t {
    sp_entity_ref_t head; // 16 bytes
    const uint8_t _unknown16[24];
} sp_ecs_character_controller_t; // 40 bytes
const uint64_t SP_CHARACTER_CONTROLLER_INDEX = 10;
const uint64_t SP_ACCESS_CHARACTER_CONTROLLER = 2ull << 10;
SP_EXPORT sp_ecs_character_controller_t *sp_entity_set_character_controller(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_character_controller_t *sp_entity_get_character_controller(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_character_controller_t *sp_entity_get_const_character_controller(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_character_controller(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_FOCUS_LOCK = 137;
// Component: focus_lock
typedef struct sp_ecs_focus_lock_t {
    const uint8_t _unknown0[4];
} sp_ecs_focus_lock_t; // 4 bytes
const uint64_t SP_FOCUS_LOCK_INDEX = 11;
const uint64_t SP_ACCESS_FOCUS_LOCK = 2ull << 11;
SP_EXPORT sp_ecs_focus_lock_t *sp_ecs_set_focus_lock(tecs_lock_t *dynLockPtr);
SP_EXPORT sp_ecs_focus_lock_t *sp_ecs_get_focus_lock(tecs_lock_t *dynLockPtr);
SP_EXPORT const sp_ecs_focus_lock_t *sp_ecs_get_const_focus_lock(tecs_lock_t *dynLockPtr);
SP_EXPORT void sp_ecs_unset_focus_lock(tecs_lock_t *dynLockPtr);
const uint32_t SP_TYPE_INDEX_FOCUS_LAYER = 96;
// Enum: ecs::FocusLayer
typedef enum sp_focus_layer_t {
    SP_FOCUS_LAYER_NEVER = 0,
    SP_FOCUS_LAYER_GAME = 1,
    SP_FOCUS_LAYER_HUD = 2,
    SP_FOCUS_LAYER_MENU = 3,
    SP_FOCUS_LAYER_OVERLAY = 4,
    SP_FOCUS_LAYER_ALWAYS = 5,
} sp_focus_layer_t;
SP_EXPORT bool sp_ecs_focus_lock_acquire_focus(sp_ecs_focus_lock_t *self, sp_focus_layer_t layer);
SP_EXPORT void sp_ecs_focus_lock_release_focus(sp_ecs_focus_lock_t *self, sp_focus_layer_t layer);
SP_EXPORT bool sp_ecs_focus_lock_has_primary_focus(const sp_ecs_focus_lock_t *self, sp_focus_layer_t layer);
SP_EXPORT bool sp_ecs_focus_lock_has_focus(const sp_ecs_focus_lock_t *self, sp_focus_layer_t layer);
SP_EXPORT void sp_ecs_focus_lock_primary_focus(const sp_ecs_focus_lock_t *self, sp_focus_layer_t *result);

const uint32_t SP_TYPE_INDEX_ECS_GUI_ELEMENT = 138;
const uint32_t SP_TYPE_INDEX_GUI_LAYOUT_ANCHOR = 97;
// Enum: ecs::GuiLayoutAnchor
typedef enum sp_gui_layout_anchor_t {
    SP_GUI_LAYOUT_ANCHOR_FULLSCREEN = 0,
    SP_GUI_LAYOUT_ANCHOR_TOP = 1,
    SP_GUI_LAYOUT_ANCHOR_LEFT = 2,
    SP_GUI_LAYOUT_ANCHOR_RIGHT = 3,
    SP_GUI_LAYOUT_ANCHOR_BOTTOM = 4,
    SP_GUI_LAYOUT_ANCHOR_FLOATING = 5,
} sp_gui_layout_anchor_t;
const uint32_t SP_TYPE_INDEX_IVEC2 = 31;
typedef struct ivec2_t { int32_t v[2]; } ivec2_t;
// Component: gui_element
typedef struct sp_ecs_gui_element_t {
    sp_gui_layout_anchor_t anchor; // 4 bytes
    ivec2_t preferred_size; // 8 bytes
    bool enabled; // 1 bytes
    const uint8_t _unknown13[19];
} sp_ecs_gui_element_t; // 32 bytes
const uint64_t SP_GUI_ELEMENT_INDEX = 12;
const uint64_t SP_ACCESS_GUI_ELEMENT = 2ull << 12;
SP_EXPORT sp_ecs_gui_element_t *sp_entity_set_gui_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_gui_element_t *sp_entity_get_gui_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_gui_element_t *sp_entity_get_const_gui_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_gui_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_LASER_EMITTER = 139;
const uint32_t SP_TYPE_INDEX_COLOR = 37;
typedef struct sp_color_t { float rgb[3]; } sp_color_t;
// Component: laser_emitter
typedef struct sp_ecs_laser_emitter_t {
    float intensity; // 4 bytes
    sp_color_t color; // 12 bytes
    bool on; // 1 bytes
    const uint8_t _unknown17[3];
    float start_distance; // 4 bytes
} sp_ecs_laser_emitter_t; // 24 bytes
const uint64_t SP_LASER_EMITTER_INDEX = 13;
const uint64_t SP_ACCESS_LASER_EMITTER = 2ull << 13;
SP_EXPORT sp_ecs_laser_emitter_t *sp_entity_set_laser_emitter(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_laser_emitter_t *sp_entity_get_laser_emitter(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_laser_emitter_t *sp_entity_get_const_laser_emitter(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_laser_emitter(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_LASER_LINE = 140;
// Component: laser_line
typedef struct sp_ecs_laser_line_t {
    const uint8_t _unknown0[48];
    float intensity; // 4 bytes
    float media_density; // 4 bytes
    bool on; // 1 bytes
    bool relative; // 1 bytes
    const uint8_t _unknown58[2];
    float radius; // 4 bytes
} sp_ecs_laser_line_t; // 64 bytes
const uint64_t SP_LASER_LINE_INDEX = 14;
const uint64_t SP_ACCESS_LASER_LINE = 2ull << 14;
SP_EXPORT sp_ecs_laser_line_t *sp_entity_set_laser_line(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_laser_line_t *sp_entity_get_laser_line(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_laser_line_t *sp_entity_get_const_laser_line(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_laser_line(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_LASER_SENSOR = 141;
// Component: laser_sensor
typedef struct sp_ecs_laser_sensor_t {
    vec3_t threshold; // 12 bytes
    const uint8_t _unknown12[12];
} sp_ecs_laser_sensor_t; // 24 bytes
const uint64_t SP_LASER_SENSOR_INDEX = 15;
const uint64_t SP_ACCESS_LASER_SENSOR = 2ull << 15;
SP_EXPORT sp_ecs_laser_sensor_t *sp_entity_set_laser_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_laser_sensor_t *sp_entity_get_laser_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_laser_sensor_t *sp_entity_get_const_laser_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_laser_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_LIGHT = 142;
const uint32_t SP_TYPE_INDEX_ANGLE = 25;
typedef struct sp_angle_t { float radians; } sp_angle_t;
const uint32_t SP_TYPE_INDEX_UINT32 = 24;
// Component: light
typedef struct sp_ecs_light_t {
    float intensity; // 4 bytes
    float illuminance; // 4 bytes
    sp_angle_t spot_angle; // 4 bytes
    sp_color_t tint; // 12 bytes
    event_name_t filter; // 128 bytes
    bool on; // 1 bytes
    const uint8_t _unknown153[3];
    uint32_t shadow_map_size; // 4 bytes
    vec2_t shadow_map_clip; // 8 bytes
} sp_ecs_light_t; // 168 bytes
const uint64_t SP_LIGHT_INDEX = 16;
const uint64_t SP_ACCESS_LIGHT = 2ull << 16;
SP_EXPORT sp_ecs_light_t *sp_entity_set_light(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_light_t *sp_entity_get_light(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_light_t *sp_entity_get_const_light(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_light(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_LIGHT_SENSOR = 143;
// Component: light_sensor
typedef struct sp_ecs_light_sensor_t {
    vec3_t position; // 12 bytes
    vec3_t direction; // 12 bytes
    vec3_t color_value; // 12 bytes
} sp_ecs_light_sensor_t; // 36 bytes
const uint64_t SP_LIGHT_SENSOR_INDEX = 17;
const uint64_t SP_ACCESS_LIGHT_SENSOR = 2ull << 17;
SP_EXPORT sp_ecs_light_sensor_t *sp_entity_set_light_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_light_sensor_t *sp_entity_get_light_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_light_sensor_t *sp_entity_get_const_light_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_light_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_OPTICAL_ELEMENT = 144;
// Component: optic
typedef struct sp_ecs_optical_element_t {
    sp_color_t pass_tint; // 12 bytes
    sp_color_t reflect_tint; // 12 bytes
    bool single_direction; // 1 bytes
    const uint8_t _unknown25[3];
} sp_ecs_optical_element_t; // 28 bytes
const uint64_t SP_OPTICAL_ELEMENT_INDEX = 18;
const uint64_t SP_ACCESS_OPTICAL_ELEMENT = 2ull << 18;
SP_EXPORT sp_ecs_optical_element_t *sp_entity_set_optical_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_optical_element_t *sp_entity_get_optical_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_optical_element_t *sp_entity_get_const_optical_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_optical_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_PHYSICS_JOINTS = 145;
const uint32_t SP_TYPE_INDEX_PHYSICS_JOINT_VECTOR = 76;
const uint32_t SP_TYPE_INDEX_PHYSICS_JOINT = 46;
const uint32_t SP_TYPE_INDEX_PHYSICS_JOINT_TYPE = 101;
// Enum: ecs::PhysicsJointType
typedef enum sp_physics_joint_type_t {
    SP_PHYSICS_JOINT_TYPE_FIXED = 0,
    SP_PHYSICS_JOINT_TYPE_DISTANCE = 1,
    SP_PHYSICS_JOINT_TYPE_SPHERICAL = 2,
    SP_PHYSICS_JOINT_TYPE_HINGE = 3,
    SP_PHYSICS_JOINT_TYPE_SLIDER = 4,
    SP_PHYSICS_JOINT_TYPE_FORCE = 5,
    SP_PHYSICS_JOINT_TYPE_NO_CLIP = 6,
    SP_PHYSICS_JOINT_TYPE_TEMPORARY_NO_CLIP = 7,
} sp_physics_joint_type_t;
// Type: ecs::PhysicsJoint
typedef struct sp_physics_joint_t {
    sp_entity_ref_t target; // 16 bytes
    sp_physics_joint_type_t type; // 4 bytes
    vec2_t limit; // 8 bytes
    sp_transform_t local_offset; // 60 bytes
    sp_transform_t remote_offset; // 60 bytes
    const uint8_t _unknown148[4];
} sp_physics_joint_t; // 152 bytes

typedef struct sp_physics_joint_vector_t {
    const uint8_t _unknown[24];
} sp_physics_joint_vector_t;
SP_EXPORT size_t sp_physics_joint_vector_get_size(const sp_physics_joint_vector_t *v);
SP_EXPORT const sp_physics_joint_t *sp_physics_joint_vector_get_const_data(const sp_physics_joint_vector_t *v);
SP_EXPORT sp_physics_joint_t *sp_physics_joint_vector_get_data(sp_physics_joint_vector_t *v);
SP_EXPORT sp_physics_joint_t *sp_physics_joint_vector_resize(sp_physics_joint_vector_t *v, size_t new_size);

// Component: physics_joints
typedef struct sp_ecs_physics_joints_t {
    sp_physics_joint_vector_t physics_joints; // 24 bytes
} sp_ecs_physics_joints_t; // 24 bytes
const uint64_t SP_PHYSICS_JOINTS_INDEX = 19;
const uint64_t SP_ACCESS_PHYSICS_JOINTS = 2ull << 19;
SP_EXPORT sp_ecs_physics_joints_t *sp_entity_set_physics_joints(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_physics_joints_t *sp_entity_get_physics_joints(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_physics_joints_t *sp_entity_get_const_physics_joints(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_physics_joints(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_PHYSICS_QUERY = 146;
// Component: physics_query
typedef struct sp_ecs_physics_query_t {
    const uint8_t _unknown0[24];
} sp_ecs_physics_query_t; // 24 bytes
const uint64_t SP_PHYSICS_QUERY_INDEX = 20;
const uint64_t SP_ACCESS_PHYSICS_QUERY = 2ull << 20;
SP_EXPORT sp_ecs_physics_query_t *sp_entity_set_physics_query(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_physics_query_t *sp_entity_get_physics_query(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_physics_query_t *sp_entity_get_const_physics_query(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_physics_query(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_RENDER_OUTPUT = 147;
const uint32_t SP_TYPE_INDEX_SIGNAL_EXPRESSION = 53;
// Type: ecs::SignalExpression
typedef struct sp_signal_expression_t {
    const uint8_t _unknown0[168];
} sp_signal_expression_t; // 168 bytes

const uint32_t SP_TYPE_INDEX_ENTITY_REF_VECTOR = 81;
typedef struct sp_entity_ref_vector_t {
    const uint8_t _unknown[24];
} sp_entity_ref_vector_t;
SP_EXPORT size_t sp_entity_ref_vector_get_size(const sp_entity_ref_vector_t *v);
SP_EXPORT const sp_entity_ref_t *sp_entity_ref_vector_get_const_data(const sp_entity_ref_vector_t *v);
SP_EXPORT sp_entity_ref_t *sp_entity_ref_vector_get_data(sp_entity_ref_vector_t *v);
SP_EXPORT sp_entity_ref_t *sp_entity_ref_vector_resize(sp_entity_ref_vector_t *v, size_t new_size);

// Component: render_output
typedef struct sp_ecs_render_output_t {
    event_name_t source; // 128 bytes
    ivec2_t output_size; // 8 bytes
    vec2_t scale; // 8 bytes
    event_name_t effect; // 128 bytes
    sp_signal_expression_t effect_if; // 168 bytes
    sp_entity_ref_vector_t gui_elements; // 24 bytes
    const uint8_t _unknown464[16];
} sp_ecs_render_output_t; // 480 bytes
const uint64_t SP_RENDER_OUTPUT_INDEX = 21;
const uint64_t SP_ACCESS_RENDER_OUTPUT = 2ull << 21;
SP_EXPORT sp_ecs_render_output_t *sp_entity_set_render_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_render_output_t *sp_entity_get_render_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_render_output_t *sp_entity_get_const_render_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_render_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_SCENE_CONNECTION = 148;
const uint32_t SP_TYPE_INDEX_STRING_63_SIGNAL_EXPRESSION_VECTOR_MAP = 93;
const uint32_t SP_TYPE_INDEX_SIGNAL_EXPRESSION_VECTOR = 69;
typedef struct sp_signal_expression_vector_t {
    const uint8_t _unknown[24];
} sp_signal_expression_vector_t;
SP_EXPORT size_t sp_signal_expression_vector_get_size(const sp_signal_expression_vector_t *v);
SP_EXPORT const sp_signal_expression_t *sp_signal_expression_vector_get_const_data(const sp_signal_expression_vector_t *v);
SP_EXPORT sp_signal_expression_t *sp_signal_expression_vector_get_data(sp_signal_expression_vector_t *v);
SP_EXPORT sp_signal_expression_t *sp_signal_expression_vector_resize(sp_signal_expression_vector_t *v, size_t new_size);

typedef void sp_string_63_signal_expression_vector_map_t;
// Component: scene_connection
typedef void sp_ecs_scene_connection_t; // unknown size
const uint64_t SP_SCENE_CONNECTION_INDEX = 22;
const uint64_t SP_ACCESS_SCENE_CONNECTION = 2ull << 22;
SP_EXPORT sp_ecs_scene_connection_t *sp_entity_set_scene_connection(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_scene_connection_t *sp_entity_get_scene_connection(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_scene_connection_t *sp_entity_get_const_scene_connection(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_scene_connection(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_SCREEN = 149;
// Component: screen
typedef struct sp_ecs_screen_t {
    event_name_t texture; // 128 bytes
    vec3_t luminance; // 12 bytes
} sp_ecs_screen_t; // 140 bytes
const uint64_t SP_SCREEN_INDEX = 23;
const uint64_t SP_ACCESS_SCREEN = 2ull << 23;
SP_EXPORT sp_ecs_screen_t *sp_entity_set_screen(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_screen_t *sp_entity_get_screen(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_screen_t *sp_entity_get_const_screen(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_screen(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_TRIGGER_AREA = 150;
const uint32_t SP_TYPE_INDEX_TRIGGER_SHAPE = 105;
// Enum: ecs::TriggerShape
const uint8_t SP_TRIGGER_SHAPE_BOX = 0;
const uint8_t SP_TRIGGER_SHAPE_SPHERE = 1;
typedef uint8_t sp_trigger_shape_t;
// Component: trigger_area
typedef struct sp_ecs_trigger_area_t {
    sp_trigger_shape_t trigger_shape; // 1 bytes
    const uint8_t _unknown1[79];
} sp_ecs_trigger_area_t; // 80 bytes
const uint64_t SP_TRIGGER_AREA_INDEX = 24;
const uint64_t SP_ACCESS_TRIGGER_AREA = 2ull << 24;
SP_EXPORT sp_ecs_trigger_area_t *sp_entity_set_trigger_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_trigger_area_t *sp_entity_get_trigger_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_trigger_area_t *sp_entity_get_const_trigger_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_trigger_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_TRIGGER_GROUP = 151;
// Component: trigger_group
const uint8_t SP_ECS_TRIGGER_GROUP_PLAYER = 0;
const uint8_t SP_ECS_TRIGGER_GROUP_OBJECT = 1;
const uint8_t SP_ECS_TRIGGER_GROUP_MAGNETIC = 2;
typedef uint8_t sp_ecs_trigger_group_t;
const uint64_t SP_TRIGGER_GROUP_INDEX = 25;
const uint64_t SP_ACCESS_TRIGGER_GROUP = 2ull << 25;
SP_EXPORT sp_ecs_trigger_group_t *sp_entity_set_trigger_group(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_trigger_group_t *sp_entity_get_trigger_group(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_trigger_group_t *sp_entity_get_const_trigger_group(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_trigger_group(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_VIEW = 152;
// Component: view
typedef struct sp_ecs_view_t {
    ivec2_t offset; // 8 bytes
    ivec2_t extents; // 8 bytes
    sp_angle_t fov; // 4 bytes
    vec2_t clip; // 8 bytes
    sp_visibility_mask_t visibility_mask; // 4 bytes
    const uint8_t _unknown32[256];
} sp_ecs_view_t; // 288 bytes
const uint64_t SP_VIEW_INDEX = 26;
const uint64_t SP_ACCESS_VIEW = 2ull << 26;
SP_EXPORT sp_ecs_view_t *sp_entity_set_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_view_t *sp_entity_get_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_view_t *sp_entity_get_const_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_VOXEL_AREA = 153;
const uint32_t SP_TYPE_INDEX_IVEC3 = 32;
typedef struct ivec3_t { int32_t v[3]; } ivec3_t;
// Component: voxel_area
typedef struct sp_ecs_voxel_area_t {
    ivec3_t extents; // 12 bytes
} sp_ecs_voxel_area_t; // 12 bytes
const uint64_t SP_VOXEL_AREA_INDEX = 27;
const uint64_t SP_ACCESS_VOXEL_AREA = 2ull << 27;
SP_EXPORT sp_ecs_voxel_area_t *sp_entity_set_voxel_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_voxel_area_t *sp_entity_get_voxel_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_voxel_area_t *sp_entity_get_const_voxel_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_voxel_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_XR_VIEW = 154;
const uint32_t SP_TYPE_INDEX_XR_EYE = 106;
// Enum: ecs::XrEye
typedef enum sp_xr_eye_t {
    SP_XR_EYE_LEFT = 0,
    SP_XR_EYE_RIGHT = 1,
} sp_xr_eye_t;
// Component: xr_view
typedef struct sp_ecs_xr_view_t {
    sp_xr_eye_t xr_eye; // 4 bytes
} sp_ecs_xr_view_t; // 4 bytes
const uint64_t SP_XR_VIEW_INDEX = 28;
const uint64_t SP_ACCESS_XR_VIEW = 2ull << 28;
SP_EXPORT sp_ecs_xr_view_t *sp_entity_set_xr_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_xr_view_t *sp_entity_get_xr_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_xr_view_t *sp_entity_get_const_xr_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_xr_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_EVENT_INPUT = 155;
// Component: event_input
typedef struct sp_ecs_event_input_t {
    const uint8_t _unknown0[72];
} sp_ecs_event_input_t; // 72 bytes
const uint64_t SP_EVENT_INPUT_INDEX = 29;
const uint64_t SP_ACCESS_EVENT_INPUT = 2ull << 29;
SP_EXPORT sp_ecs_event_input_t *sp_entity_set_event_input(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_event_input_t *sp_entity_get_event_input(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_event_input_t *sp_entity_get_const_event_input(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_event_input(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_EVENT_BINDINGS = 156;
const uint32_t SP_TYPE_INDEX_EVENT_NAME_EVENT_BINDING_VECTOR_MAP = 94;
const uint32_t SP_TYPE_INDEX_EVENT_BINDING_VECTOR = 71;
const uint32_t SP_TYPE_INDEX_EVENT_BINDING = 43;
const uint32_t SP_TYPE_INDEX_EVENT_DEST_VECTOR = 70;
const uint32_t SP_TYPE_INDEX_EVENT_DEST = 45;
// Type: ecs::EventDest
typedef struct sp_event_dest_t {
    const uint8_t _unknown0[144];
} sp_event_dest_t; // 144 bytes

typedef struct sp_event_dest_vector_t {
    const uint8_t _unknown[24];
} sp_event_dest_vector_t;
SP_EXPORT size_t sp_event_dest_vector_get_size(const sp_event_dest_vector_t *v);
SP_EXPORT const sp_event_dest_t *sp_event_dest_vector_get_const_data(const sp_event_dest_vector_t *v);
SP_EXPORT sp_event_dest_t *sp_event_dest_vector_get_data(sp_event_dest_vector_t *v);
SP_EXPORT sp_event_dest_t *sp_event_dest_vector_resize(sp_event_dest_vector_t *v, size_t new_size);

const uint32_t SP_TYPE_INDEX_EVENT_BINDING_ACTIONS = 44;
const uint32_t SP_TYPE_INDEX_OPTIONAL_SIGNAL_EXPRESSION = 86;
typedef struct sp_optional_signal_expression_t {
    const uint8_t _unknown[176];
} sp_optional_signal_expression_t;

const uint32_t SP_TYPE_INDEX_OPTIONAL_EVENT_DATA = 85;
const uint32_t SP_TYPE_INDEX_EVENT_DATA = 5;
const uint32_t SP_TYPE_INDEX_EVENT_DATA_TYPE = 6;
// Enum: ecs::EventDataType
typedef enum sp_event_data_type_t {
    SP_EVENT_DATA_TYPE_BOOL = 0,
    SP_EVENT_DATA_TYPE_INT = 1,
    SP_EVENT_DATA_TYPE_UINT = 2,
    SP_EVENT_DATA_TYPE_FLOAT = 3,
    SP_EVENT_DATA_TYPE_DOUBLE = 4,
    SP_EVENT_DATA_TYPE_VEC2 = 5,
    SP_EVENT_DATA_TYPE_VEC3 = 6,
    SP_EVENT_DATA_TYPE_VEC4 = 7,
    SP_EVENT_DATA_TYPE_TRANSFORM = 8,
    SP_EVENT_DATA_TYPE_NAMED_ENTITY = 9,
    SP_EVENT_DATA_TYPE_ENTITY = 10,
    SP_EVENT_DATA_TYPE_STRING = 11,
    SP_EVENT_DATA_TYPE_BYTES = 12,
} sp_event_data_type_t;
const uint32_t SP_TYPE_INDEX_INT32 = 23;
const uint32_t SP_TYPE_INDEX_VEC4 = 27;
typedef struct vec4_t { float v[4]; } vec4_t;
const uint32_t SP_TYPE_INDEX_NAMED_ENTITY = 18;
// Type: ecs::NamedEntity
typedef struct sp_named_entity_t {
    const uint8_t _unknown0[136];
} sp_named_entity_t; // 136 bytes
SP_EXPORT void sp_named_entity_name(const sp_named_entity_t *self, sp_ecs_name_t *result);
SP_EXPORT tecs_entity_t sp_named_entity_get(const sp_named_entity_t *self, const tecs_lock_t * lock);
SP_EXPORT bool sp_named_entity_is_valid(const sp_named_entity_t *self);
SP_EXPORT void sp_named_entity_find(const char * ent, const sp_ecs_name_t * arg1, sp_named_entity_t *result);
SP_EXPORT void sp_named_entity_lookup(tecs_entity_t name, sp_named_entity_t *result);
SP_EXPORT void sp_named_entity_clear(sp_named_entity_t *self);

const uint32_t SP_TYPE_INDEX_EVENT_BYTES = 11;
typedef uint8_t event_bytes_t[256];
// Type: ecs::EventData
typedef struct sp_event_data_t {
    sp_event_data_type_t type; // 4 bytes
    const uint8_t _unknown4[4];
    union {
        bool b; // 1 bytes
        int32_t i; // 4 bytes
        uint32_t ui; // 4 bytes
        float f; // 4 bytes
        double d; // 8 bytes
        vec2_t vec2; // 8 bytes
        vec3_t vec3; // 12 bytes
        vec4_t vec4; // 16 bytes
        sp_transform_t transform; // 60 bytes
        sp_named_entity_t namedEntity; // 136 bytes
        tecs_entity_t ent; // 8 bytes
        event_string_t str; // 256 bytes
        event_bytes_t bytes; // 256 bytes
    }; // 256 bytes
} sp_event_data_t; // 264 bytes

typedef struct sp_optional_event_data_t {
    const uint8_t _unknown[272];
} sp_optional_event_data_t;

// Type: ecs::EventBindingActions
typedef struct sp_event_binding_actions_t {
    sp_optional_signal_expression_t filter; // 176 bytes
    sp_signal_expression_vector_t modify; // 24 bytes
    sp_optional_event_data_t set_value; // 272 bytes
} sp_event_binding_actions_t; // 472 bytes

// Type: ecs::EventBinding
typedef struct sp_event_binding_t {
    sp_event_dest_vector_t outputs; // 24 bytes
    sp_event_binding_actions_t event_binding_actions; // 472 bytes
} sp_event_binding_t; // 496 bytes

typedef struct sp_event_binding_vector_t {
    const uint8_t _unknown[24];
} sp_event_binding_vector_t;
SP_EXPORT size_t sp_event_binding_vector_get_size(const sp_event_binding_vector_t *v);
SP_EXPORT const sp_event_binding_t *sp_event_binding_vector_get_const_data(const sp_event_binding_vector_t *v);
SP_EXPORT sp_event_binding_t *sp_event_binding_vector_get_data(sp_event_binding_vector_t *v);
SP_EXPORT sp_event_binding_t *sp_event_binding_vector_resize(sp_event_binding_vector_t *v, size_t new_size);

typedef void sp_event_name_event_binding_vector_map_t;
// Component: event_bindings
typedef void sp_ecs_event_bindings_t; // unknown size
const uint64_t SP_EVENT_BINDINGS_INDEX = 30;
const uint64_t SP_ACCESS_EVENT_BINDINGS = 2ull << 30;
SP_EXPORT sp_ecs_event_bindings_t *sp_entity_set_event_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_event_bindings_t *sp_entity_get_event_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_event_bindings_t *sp_entity_get_const_event_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_event_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_SIGNALS = 157;
// Component: signals
typedef struct sp_ecs_signals_t {
    const uint8_t _unknown0[88];
} sp_ecs_signals_t; // 88 bytes
const uint64_t SP_SIGNALS_INDEX = 31;
const uint64_t SP_ACCESS_SIGNALS = 2ull << 31;
SP_EXPORT sp_ecs_signals_t *sp_ecs_set_signals(tecs_lock_t *dynLockPtr);
SP_EXPORT sp_ecs_signals_t *sp_ecs_get_signals(tecs_lock_t *dynLockPtr);
SP_EXPORT const sp_ecs_signals_t *sp_ecs_get_const_signals(tecs_lock_t *dynLockPtr);
SP_EXPORT void sp_ecs_unset_signals(tecs_lock_t *dynLockPtr);

const uint32_t SP_TYPE_INDEX_ECS_SIGNAL_OUTPUT = 158;
const uint32_t SP_TYPE_INDEX_EVENT_NAME_DOUBLE_MAP = 88;
typedef void sp_event_name_double_map_t;
// Component: signal_output
typedef void sp_ecs_signal_output_t; // unknown size
const uint64_t SP_SIGNAL_OUTPUT_INDEX = 32;
const uint64_t SP_ACCESS_SIGNAL_OUTPUT = 2ull << 32;
SP_EXPORT sp_ecs_signal_output_t *sp_entity_set_signal_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_signal_output_t *sp_entity_get_signal_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_signal_output_t *sp_entity_get_const_signal_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_signal_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_SIGNAL_BINDINGS = 159;
const uint32_t SP_TYPE_INDEX_EVENT_NAME_SIGNAL_EXPRESSION_MAP = 90;
typedef void sp_event_name_signal_expression_map_t;
// Component: signal_bindings
typedef void sp_ecs_signal_bindings_t; // unknown size
const uint64_t SP_SIGNAL_BINDINGS_INDEX = 33;
const uint64_t SP_ACCESS_SIGNAL_BINDINGS = 2ull << 33;
SP_EXPORT sp_ecs_signal_bindings_t *sp_entity_set_signal_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_signal_bindings_t *sp_entity_get_signal_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_signal_bindings_t *sp_entity_get_const_signal_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_signal_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_ECS_SCRIPTS = 160;
const uint32_t SP_TYPE_INDEX_SCRIPT_INSTANCE_VECTOR = 78;
const uint32_t SP_TYPE_INDEX_SCRIPT_INSTANCE = 51;
// Type: ecs::ScriptInstance
typedef struct sp_script_instance_t {
    const uint8_t _unknown0[16];
} sp_script_instance_t; // 16 bytes
const uint32_t SP_TYPE_INDEX_SCRIPT_STATE = 52;
const uint32_t SP_TYPE_INDEX_SCRIPT_DEFINITION = 49;
const uint32_t SP_TYPE_INDEX_STRING = 7;
typedef struct string_t { const uint8_t _unknown[24]; } string_t;
SP_EXPORT void sp_string_set(string_t *str, const char *new_str);
SP_EXPORT int sp_string_compare(const string_t *str, const char *other_str);
SP_EXPORT size_t sp_string_get_size(const string_t *str);
SP_EXPORT const char *sp_string_get_c_str(const string_t *str);
SP_EXPORT char *sp_string_get_data(string_t *str);
SP_EXPORT char *sp_string_resize(string_t *str, size_t new_size, char fill_char);

const uint32_t SP_TYPE_INDEX_SCRIPT_TYPE = 103;
// Enum: ecs::ScriptType
typedef enum sp_script_type_t {
    SP_SCRIPT_TYPE_LOGIC_SCRIPT = 0,
    SP_SCRIPT_TYPE_PHYSICS_SCRIPT = 1,
    SP_SCRIPT_TYPE_EVENT_SCRIPT = 2,
    SP_SCRIPT_TYPE_PREFAB_SCRIPT = 3,
    SP_SCRIPT_TYPE_GUI_SCRIPT = 4,
} sp_script_type_t;
const uint32_t SP_TYPE_INDEX_EVENT_NAME_VECTOR = 67;
typedef struct sp_event_name_vector_t {
    const uint8_t _unknown[24];
} sp_event_name_vector_t;
SP_EXPORT size_t sp_event_name_vector_get_size(const sp_event_name_vector_t *v);
SP_EXPORT const event_name_t *sp_event_name_vector_get_const_data(const sp_event_name_vector_t *v);
SP_EXPORT event_name_t *sp_event_name_vector_get_data(sp_event_name_vector_t *v);
SP_EXPORT event_name_t *sp_event_name_vector_resize(sp_event_name_vector_t *v, size_t new_size);

// Type: ecs::ScriptDefinition
typedef struct sp_script_definition_t {
    string_t name; // 24 bytes
    sp_script_type_t type; // 4 bytes
    const uint8_t _unknown28[4];
    sp_event_name_vector_t events; // 24 bytes
    bool filter_on_event; // 1 bytes
    const uint8_t _unknown57[79];
} sp_script_definition_t; // 136 bytes

// Type: ecs::ScriptState
typedef struct sp_script_state_t {
    sp_ecs_name_t scope; // 128 bytes
    sp_script_definition_t definition; // 136 bytes
    const uint8_t _unknown264[472];
} sp_script_state_t; // 736 bytes
const uint32_t SP_TYPE_INDEX_EVENT = 42;
// Type: ecs::Event
typedef struct sp_event_t {
    event_name_t name; // 128 bytes
    tecs_entity_t source; // 8 bytes
    sp_event_data_t data; // 264 bytes
} sp_event_t; // 400 bytes
SP_EXPORT uint64_t sp_event_send(const tecs_lock_t * lock, tecs_entity_t target, const sp_event_t * event);
SP_EXPORT uint64_t sp_event_send_ref(const tecs_lock_t * lock, const sp_entity_ref_t * target, const sp_event_t * event);

SP_EXPORT sp_event_t * sp_script_state_poll_event(sp_script_state_t *self, const tecs_lock_t * lock);

SP_EXPORT sp_script_state_t * sp_script_instance_get_state(const sp_script_instance_t *self);

typedef struct sp_script_instance_vector_t {
    const uint8_t _unknown[24];
} sp_script_instance_vector_t;
SP_EXPORT size_t sp_script_instance_vector_get_size(const sp_script_instance_vector_t *v);
SP_EXPORT const sp_script_instance_t *sp_script_instance_vector_get_const_data(const sp_script_instance_vector_t *v);
SP_EXPORT sp_script_instance_t *sp_script_instance_vector_get_data(sp_script_instance_vector_t *v);
SP_EXPORT sp_script_instance_t *sp_script_instance_vector_resize(sp_script_instance_vector_t *v, size_t new_size);

// Component: Scripts
typedef struct sp_ecs_scripts_t {
    sp_script_instance_vector_t script_instances; // 24 bytes
} sp_ecs_scripts_t; // 24 bytes
const uint64_t SP_SCRIPTS_INDEX = 34;
const uint64_t SP_ACCESS_SCRIPTS = 2ull << 34;
SP_EXPORT sp_ecs_scripts_t *sp_entity_set_scripts(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_scripts_t *sp_entity_get_scripts(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_scripts_t *sp_entity_get_const_scripts(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_scripts(tecs_lock_t *dynLockPtr, sp_entity_t ent);

const uint32_t SP_TYPE_INDEX_UINT8 = 21;
const uint32_t SP_TYPE_INDEX_UINT16 = 22;
const uint32_t SP_TYPE_INDEX_DVEC2 = 28;
typedef struct dvec2_t { double v[2]; } dvec2_t;
const uint32_t SP_TYPE_INDEX_DVEC3 = 29;
typedef struct dvec3_t { double v[3]; } dvec3_t;
const uint32_t SP_TYPE_INDEX_DVEC4 = 30;
typedef struct dvec4_t { double v[4]; } dvec4_t;
const uint32_t SP_TYPE_INDEX_IVEC4 = 33;
typedef struct ivec4_t { int32_t v[4]; } ivec4_t;
const uint32_t SP_TYPE_INDEX_UVEC2 = 34;
typedef struct uvec2_t { uint32_t v[2]; } uvec2_t;
const uint32_t SP_TYPE_INDEX_UVEC3 = 35;
typedef struct uvec3_t { uint32_t v[3]; } uvec3_t;
const uint32_t SP_TYPE_INDEX_UVEC4 = 36;
typedef struct uvec4_t { uint32_t v[4]; } uvec4_t;
const uint32_t SP_TYPE_INDEX_DYNAMIC_SCRIPT_DEFINITION = 41;
const uint32_t SP_TYPE_INDEX_STRUCT_FIELD_VECTOR = 80;
const uint32_t SP_TYPE_INDEX_STRUCT_FIELD = 56;
const uint32_t SP_TYPE_INDEX_TYPE_INFO = 62;
// Type: ecs::TypeInfo
typedef struct sp_type_info_t {
    uint32_t type_index; // 4 bytes
    bool is_trivial; // 1 bytes
    bool is_const; // 1 bytes
    bool is_pointer; // 1 bytes
    bool is_reference; // 1 bytes
    bool is_tecs_lock; // 1 bytes
    bool is_function_pointer; // 1 bytes
    const uint8_t _unknown10[2];
} sp_type_info_t; // 12 bytes

const uint32_t SP_TYPE_INDEX_FIELD_ACTION = 95;
// Enum: ecs::FieldAction
typedef enum sp_field_action_t {
    SP_FIELD_ACTION_AUTO_LOAD = 1,
    SP_FIELD_ACTION_AUTO_SAVE = 2,
    SP_FIELD_ACTION_AUTO_APPLY = 4,
} sp_field_action_t;
// Type: ecs::StructField
typedef struct sp_struct_field_t {
    string_t name; // 24 bytes
    string_t desc; // 24 bytes
    sp_type_info_t type; // 12 bytes
    const uint8_t _unknown60[4];
    uint64_t size; // 8 bytes
    uint64_t offset; // 8 bytes
    const uint8_t _unknown80[4];
    sp_field_action_t actions; // 4 bytes
    const uint8_t _unknown88[128];
} sp_struct_field_t; // 216 bytes

typedef struct sp_struct_field_vector_t {
    const uint8_t _unknown[24];
} sp_struct_field_vector_t;
SP_EXPORT size_t sp_struct_field_vector_get_size(const sp_struct_field_vector_t *v);
SP_EXPORT const sp_struct_field_t *sp_struct_field_vector_get_const_data(const sp_struct_field_vector_t *v);
SP_EXPORT sp_struct_field_t *sp_struct_field_vector_get_data(sp_struct_field_vector_t *v);
SP_EXPORT sp_struct_field_t *sp_struct_field_vector_resize(sp_struct_field_vector_t *v, size_t new_size);

const uint32_t SP_TYPE_INDEX_TECS_LOCK = 107;
const uint32_t SP_TYPE_INDEX_COMPOSITOR_CTX = 57;
const uint32_t SP_TYPE_INDEX_GUI_DRAW_DATA = 58;
const uint32_t SP_TYPE_INDEX_GUI_DRAW_COMMAND_VECTOR = 72;
const uint32_t SP_TYPE_INDEX_GUI_DRAW_COMMAND = 59;
// Type: sp::GuiDrawCommand
typedef struct sp_gui_draw_command_t {
    vec4_t clipRect; // 16 bytes
    uint64_t textureId; // 8 bytes
    uint32_t indexCount; // 4 bytes
    uint32_t vertexOffset; // 4 bytes
} sp_gui_draw_command_t; // 32 bytes

typedef struct sp_gui_draw_command_vector_t {
    const uint8_t _unknown[24];
} sp_gui_draw_command_vector_t;
SP_EXPORT size_t sp_gui_draw_command_vector_get_size(const sp_gui_draw_command_vector_t *v);
SP_EXPORT const sp_gui_draw_command_t *sp_gui_draw_command_vector_get_const_data(const sp_gui_draw_command_vector_t *v);
SP_EXPORT sp_gui_draw_command_t *sp_gui_draw_command_vector_get_data(sp_gui_draw_command_vector_t *v);
SP_EXPORT sp_gui_draw_command_t *sp_gui_draw_command_vector_resize(sp_gui_draw_command_vector_t *v, size_t new_size);

const uint32_t SP_TYPE_INDEX_UINT16_VECTOR = 73;
typedef struct sp_uint16_vector_t {
    const uint8_t _unknown[24];
} sp_uint16_vector_t;
SP_EXPORT size_t sp_uint16_vector_get_size(const sp_uint16_vector_t *v);
SP_EXPORT const uint16_t *sp_uint16_vector_get_const_data(const sp_uint16_vector_t *v);
SP_EXPORT uint16_t *sp_uint16_vector_get_data(sp_uint16_vector_t *v);
SP_EXPORT uint16_t *sp_uint16_vector_resize(sp_uint16_vector_t *v, size_t new_size);

const uint32_t SP_TYPE_INDEX_GUI_DRAW_VERTEX_VECTOR = 74;
const uint32_t SP_TYPE_INDEX_GUI_DRAW_VERTEX = 60;
// Type: sp::GuiDrawVertex
typedef struct sp_gui_draw_vertex_t {
    vec2_t pos; // 8 bytes
    vec2_t uv; // 8 bytes
    uint32_t col; // 4 bytes
} sp_gui_draw_vertex_t; // 20 bytes

typedef struct sp_gui_draw_vertex_vector_t {
    const uint8_t _unknown[24];
} sp_gui_draw_vertex_vector_t;
SP_EXPORT size_t sp_gui_draw_vertex_vector_get_size(const sp_gui_draw_vertex_vector_t *v);
SP_EXPORT const sp_gui_draw_vertex_t *sp_gui_draw_vertex_vector_get_const_data(const sp_gui_draw_vertex_vector_t *v);
SP_EXPORT sp_gui_draw_vertex_t *sp_gui_draw_vertex_vector_get_data(sp_gui_draw_vertex_vector_t *v);
SP_EXPORT sp_gui_draw_vertex_t *sp_gui_draw_vertex_vector_resize(sp_gui_draw_vertex_vector_t *v, size_t new_size);

// Type: sp::GuiDrawData
typedef struct sp_gui_draw_data_t {
    sp_gui_draw_command_vector_t drawCommands; // 24 bytes
    sp_uint16_vector_t indexBuffer; // 24 bytes
    sp_gui_draw_vertex_vector_t vertexBuffer; // 24 bytes
} sp_gui_draw_data_t; // 72 bytes

// Type: ecs::DynamicScriptDefinition
typedef struct sp_dynamic_script_definition_t {
    string_t name; // 24 bytes
    char * desc; // 8 bytes
    sp_script_type_t type; // 4 bytes
    bool filter_on_event; // 1 bytes
    const uint8_t _unknown37[3];
    sp_event_name_vector_t events; // 24 bytes
    sp_struct_field_vector_t fields; // 24 bytes
    uint64_t context_size; // 8 bytes
    void(*default_init_func)(void *); // 8 bytes
    void(*default_free_func)(void *); // 8 bytes
    void(*init_func)(void *, sp_script_state_t *); // 8 bytes
    void(*destroy_func)(void *, sp_script_state_t *); // 8 bytes
    void(*on_tick_func)(void *, sp_script_state_t *, tecs_lock_t *, tecs_entity_t, uint64_t); // 8 bytes
    void(*on_event_func)(void *, sp_script_state_t *, tecs_lock_t *, tecs_entity_t, sp_event_t *); // 8 bytes
    void(*prefab_func)(const sp_script_state_t *, tecs_lock_t *, tecs_entity_t, const sp_scene_ref_t *); // 8 bytes
    bool(*before_frame_func)(void *, sp_compositor_ctx_t *, sp_script_state_t *, tecs_entity_t); // 8 bytes
    void(*render_gui_func)(void *, sp_compositor_ctx_t *, sp_script_state_t *, tecs_entity_t, vec2_t, vec2_t, float, sp_gui_draw_data_t *); // 8 bytes
} sp_dynamic_script_definition_t; // 168 bytes

const uint32_t SP_TYPE_INDEX_SCRIPT_DEFINITION_BASE = 50;
// Type: ecs::ScriptDefinitionBase
typedef struct sp_script_definition_base_t {
    const uint8_t _unknown0[16];
} sp_script_definition_base_t; // 16 bytes
SP_EXPORT void * sp_script_definition_base_access_mut(const sp_script_definition_base_t *self, sp_script_state_t * arg0);
SP_EXPORT const void * sp_script_definition_base_access(const sp_script_definition_base_t *self, const sp_script_state_t * arg0);

const uint32_t SP_TYPE_INDEX_SIGNAL_REF = 54;
// Type: ecs::SignalRef
typedef struct sp_signal_ref_t {
    const uint8_t _unknown0[16];
} sp_signal_ref_t; // 16 bytes
SP_EXPORT const sp_entity_ref_t * sp_signal_ref_get_entity(const sp_signal_ref_t *self);
SP_EXPORT const string_t * sp_signal_ref_get_signal_name(const sp_signal_ref_t *self);
SP_EXPORT void sp_signal_ref_string(const sp_signal_ref_t *self, string_t *result);
SP_EXPORT bool sp_signal_ref_is_valid(const sp_signal_ref_t *self);
SP_EXPORT double * sp_signal_ref_set_value(const sp_signal_ref_t *self, const tecs_lock_t * lock, double value);
SP_EXPORT bool sp_signal_ref_has_value(const sp_signal_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT void sp_signal_ref_clear_value(const sp_signal_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT const double * sp_signal_ref_get_value(const sp_signal_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT sp_signal_expression_t * sp_signal_ref_set_binding(const sp_signal_ref_t *self, const tecs_lock_t * lock, const sp_signal_expression_t * expr);
SP_EXPORT bool sp_signal_ref_has_binding(const sp_signal_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT void sp_signal_ref_clear_binding(const sp_signal_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT const sp_signal_expression_t * sp_signal_ref_get_binding(const sp_signal_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT double sp_signal_ref_get_signal(const sp_signal_ref_t *self, const tecs_lock_t * lock, uint32_t depth);
SP_EXPORT void sp_signal_ref_empty(sp_signal_ref_t *result);
SP_EXPORT void sp_signal_ref_new(const sp_entity_ref_t * ent, const char * signal_name, sp_signal_ref_t *result);
SP_EXPORT void sp_signal_ref_copy(const sp_signal_ref_t * ref, sp_signal_ref_t *result);
SP_EXPORT void sp_signal_ref_lookup(const char * str, const sp_ecs_name_t * scope, sp_signal_ref_t *result);
SP_EXPORT void sp_signal_ref_clear(sp_signal_ref_t *self);

const uint32_t SP_TYPE_INDEX_STRING_2 = 63;
typedef char string_2_t[3];
const uint32_t SP_TYPE_INDEX_FLOAT_VECTOR = 64;
typedef struct sp_float_vector_t {
    const uint8_t _unknown[24];
} sp_float_vector_t;
SP_EXPORT size_t sp_float_vector_get_size(const sp_float_vector_t *v);
SP_EXPORT const float *sp_float_vector_get_const_data(const sp_float_vector_t *v);
SP_EXPORT float *sp_float_vector_get_data(sp_float_vector_t *v);
SP_EXPORT float *sp_float_vector_resize(sp_float_vector_t *v, size_t new_size);

const uint32_t SP_TYPE_INDEX_VEC2_VECTOR = 65;
typedef struct sp_vec2_vector_t {
    const uint8_t _unknown[24];
} sp_vec2_vector_t;
SP_EXPORT size_t sp_vec2_vector_get_size(const sp_vec2_vector_t *v);
SP_EXPORT const vec2_t *sp_vec2_vector_get_const_data(const sp_vec2_vector_t *v);
SP_EXPORT vec2_t *sp_vec2_vector_get_data(sp_vec2_vector_t *v);
SP_EXPORT vec2_t *sp_vec2_vector_resize(sp_vec2_vector_t *v, size_t new_size);

const uint32_t SP_TYPE_INDEX_STRING_VECTOR = 66;
typedef struct sp_string_vector_t {
    const uint8_t _unknown[24];
} sp_string_vector_t;
SP_EXPORT size_t sp_string_vector_get_size(const sp_string_vector_t *v);
SP_EXPORT const string_t *sp_string_vector_get_const_data(const sp_string_vector_t *v);
SP_EXPORT string_t *sp_string_vector_get_data(sp_string_vector_t *v);
SP_EXPORT string_t *sp_string_vector_resize(sp_string_vector_t *v, size_t new_size);

const uint32_t SP_TYPE_INDEX_EVENT_STRING_VECTOR = 68;
typedef struct sp_event_string_vector_t {
    const uint8_t _unknown[24];
} sp_event_string_vector_t;
SP_EXPORT size_t sp_event_string_vector_get_size(const sp_event_string_vector_t *v);
SP_EXPORT const event_string_t *sp_event_string_vector_get_const_data(const sp_event_string_vector_t *v);
SP_EXPORT event_string_t *sp_event_string_vector_get_data(sp_event_string_vector_t *v);
SP_EXPORT event_string_t *sp_event_string_vector_resize(sp_event_string_vector_t *v, size_t new_size);

const uint32_t SP_TYPE_INDEX_ENTITY_REF_PAIR_VECTOR = 82;
const uint32_t SP_TYPE_INDEX_ENTITY_REF_PAIR = 83;
typedef struct sp_entity_ref_pair_t {
    const uint8_t _unknown[32];
} sp_entity_ref_pair_t;
typedef struct sp_entity_ref_pair_vector_t {
    const uint8_t _unknown[24];
} sp_entity_ref_pair_vector_t;
SP_EXPORT size_t sp_entity_ref_pair_vector_get_size(const sp_entity_ref_pair_vector_t *v);
SP_EXPORT const sp_entity_ref_pair_t *sp_entity_ref_pair_vector_get_const_data(const sp_entity_ref_pair_vector_t *v);
SP_EXPORT sp_entity_ref_pair_t *sp_entity_ref_pair_vector_get_data(sp_entity_ref_pair_vector_t *v);
SP_EXPORT sp_entity_ref_pair_t *sp_entity_ref_pair_vector_resize(sp_entity_ref_pair_vector_t *v, size_t new_size);

const uint32_t SP_TYPE_INDEX_OPTIONAL_DOUBLE = 84;
typedef struct sp_optional_double_t {
    const uint8_t _unknown[16];
} sp_optional_double_t;

const uint32_t SP_TYPE_INDEX_OPTIONAL_PHYSICS_ACTOR_TYPE = 87;
typedef struct sp_optional_physics_actor_type_t {
    const uint8_t _unknown[2];
} sp_optional_physics_actor_type_t;

const uint32_t SP_TYPE_INDEX_EVENT_NAME_EVENT_NAME_MAP = 89;
typedef void sp_event_name_event_name_map_t;
const uint32_t SP_TYPE_INDEX_STRING_SIGNAL_EXPRESSION_MAP = 91;
typedef void sp_string_signal_expression_map_t;
const uint32_t SP_TYPE_INDEX_EVENT_NAME_PHYSICS_JOINT_MAP = 92;
typedef void sp_event_name_physics_joint_map_t;
const uint32_t SP_TYPE_INDEX_SCENE_PRIORITY = 102;
// Enum: sp::ScenePriority
typedef enum sp_scene_priority_t {
    SP_SCENE_PRIORITY_SYSTEM = 0,
    SP_SCENE_PRIORITY_SCENE = 1,
    SP_SCENE_PRIORITY_PLAYER = 2,
    SP_SCENE_PRIORITY_SAVE_GAME = 3,
    SP_SCENE_PRIORITY_BINDINGS = 4,
    SP_SCENE_PRIORITY_OVERRIDE = 5,
} sp_scene_priority_t;
const uint32_t SP_TYPE_INDEX_VOID_PTR = 117;

#pragma pack(pop)
#ifdef __cplusplus
} // extern "C"
#endif
#else

#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cstdint>
#include <robin_hood.h>

extern "C" {
#pragma pack(push, 1)
typedef sp::InlineString<63> string_63_t;
// Component: Name
typedef ecs::Name sp_ecs_name_t;
const uint64_t SP_NAME_INDEX = 0;
const uint64_t SP_ACCESS_NAME = 2ull <<0;
SP_EXPORT sp_ecs_name_t *sp_entity_set_name(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_name_t *sp_entity_get_name(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_name_t *sp_entity_get_const_name(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_name(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: SceneInfo
typedef ecs::SceneInfo sp_ecs_scene_info_t;
const uint64_t SP_SCENE_INFO_INDEX = 1;
const uint64_t SP_ACCESS_SCENE_INFO = 2ull <<1;
SP_EXPORT sp_ecs_scene_info_t *sp_entity_set_scene_info(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_scene_info_t *sp_entity_get_scene_info(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_scene_info_t *sp_entity_get_const_scene_info(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_scene_info(tecs_lock_t *dynLockPtr, sp_entity_t ent);

typedef glm::vec3 vec3_t;
typedef glm::mat3 mat3_t;
// Type: ecs::Transform
typedef ecs::Transform sp_transform_t;
typedef glm::quat quat_t;
typedef glm::mat4 mat4_t;
SP_EXPORT void sp_transform_translate(sp_transform_t *self, const vec3_t * xyz);
SP_EXPORT void sp_transform_rotate_axis(sp_transform_t *self, float radians, const vec3_t * axis);
SP_EXPORT void sp_transform_rotate(sp_transform_t *self, const quat_t * quat);
SP_EXPORT void sp_transform_scale(sp_transform_t *self, const vec3_t * xyz);
SP_EXPORT void sp_transform_set_position(sp_transform_t *self, const vec3_t * pos);
SP_EXPORT void sp_transform_set_rotation(sp_transform_t *self, const quat_t * quat);
SP_EXPORT void sp_transform_set_scale(sp_transform_t *self, const vec3_t * xyz);
SP_EXPORT const vec3_t * sp_transform_get_position(const sp_transform_t *self);
SP_EXPORT void sp_transform_get_rotation(const sp_transform_t *self, quat_t *result);
SP_EXPORT void sp_transform_get_forward(const sp_transform_t *self, vec3_t *result);
SP_EXPORT void sp_transform_get_up(const sp_transform_t *self, vec3_t *result);
SP_EXPORT const vec3_t * sp_transform_get_scale(const sp_transform_t *self);
SP_EXPORT const sp_transform_t * sp_transform_get(const sp_transform_t *self);
SP_EXPORT void sp_transform_get_inverse(const sp_transform_t *self, sp_transform_t *result);
SP_EXPORT void sp_transform_get_matrix(const sp_transform_t *self, mat4_t *result);

// Component: SceneProperties
typedef ecs::SceneProperties sp_ecs_scene_properties_t;
const uint64_t SP_SCENE_PROPERTIES_INDEX = 2;
const uint64_t SP_ACCESS_SCENE_PROPERTIES = 2ull <<2;
SP_EXPORT sp_ecs_scene_properties_t *sp_entity_set_scene_properties(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_scene_properties_t *sp_entity_get_scene_properties(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_scene_properties_t *sp_entity_get_const_scene_properties(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_scene_properties(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: TransformSnapshot
typedef ecs::TransformSnapshot sp_ecs_transform_snapshot_t;
const uint64_t SP_TRANSFORM_SNAPSHOT_INDEX = 3;
const uint64_t SP_ACCESS_TRANSFORM_SNAPSHOT = 2ull <<3;
SP_EXPORT sp_ecs_transform_snapshot_t *sp_entity_set_transform_snapshot(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_transform_snapshot_t *sp_entity_get_transform_snapshot(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_transform_snapshot_t *sp_entity_get_const_transform_snapshot(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_transform_snapshot(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Type: ecs::EntityRef
typedef ecs::EntityRef sp_entity_ref_t;
SP_EXPORT void sp_entity_ref_name(const sp_entity_ref_t *self, sp_ecs_name_t *result);
SP_EXPORT tecs_entity_t sp_entity_ref_get(const sp_entity_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT bool sp_entity_ref_is_valid(const sp_entity_ref_t *self);
SP_EXPORT void sp_entity_ref_empty(sp_entity_ref_t *result);
SP_EXPORT void sp_entity_ref_new(tecs_entity_t ent, sp_entity_ref_t *result);
SP_EXPORT void sp_entity_ref_copy(const sp_entity_ref_t * ref, sp_entity_ref_t *result);
SP_EXPORT void sp_entity_ref_lookup(const char * name, const sp_ecs_name_t * scope, sp_entity_ref_t *result);
SP_EXPORT void sp_entity_ref_clear(sp_entity_ref_t *self);

// Component: TransformTree
typedef ecs::TransformTree sp_ecs_transform_tree_t;
const uint64_t SP_TRANSFORM_TREE_INDEX = 4;
const uint64_t SP_ACCESS_TRANSFORM_TREE = 2ull <<4;
SP_EXPORT sp_ecs_transform_tree_t *sp_entity_set_transform_tree(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_transform_tree_t *sp_entity_get_transform_tree(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_transform_tree_t *sp_entity_get_const_transform_tree(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_transform_tree(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_ecs_transform_tree_move_via_root(tecs_lock_t * arg0, tecs_entity_t arg1, sp_transform_t arg2);
SP_EXPORT tecs_entity_t sp_ecs_transform_tree_get_root(tecs_lock_t * arg0, tecs_entity_t arg1);
SP_EXPORT void sp_ecs_transform_tree_get_global_transform(const sp_ecs_transform_tree_t *self, tecs_lock_t * arg0, sp_transform_t *result);
SP_EXPORT void sp_ecs_transform_tree_get_global_rotation(const sp_ecs_transform_tree_t *self, tecs_lock_t * arg0, quat_t *result);
SP_EXPORT void sp_ecs_transform_tree_get_relative_transform(const sp_ecs_transform_tree_t *self, tecs_lock_t * arg0, const tecs_entity_t * arg1, sp_transform_t *result);

typedef ecs::EventString event_string_t;
// Enum: ecs::VisibilityMask
typedef ecs::VisibilityMask sp_visibility_mask_t;
typedef sp::color_alpha_t sp_color_alpha_t;
typedef ecs::EventName event_name_t;
typedef glm::vec2 vec2_t;
// Component: renderable
typedef ecs::Renderable sp_ecs_renderable_t;
const uint64_t SP_RENDERABLE_INDEX = 5;
const uint64_t SP_ACCESS_RENDERABLE = 2ull <<5;
SP_EXPORT sp_ecs_renderable_t *sp_entity_set_renderable(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_renderable_t *sp_entity_get_renderable(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_renderable_t *sp_entity_get_const_renderable(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_renderable(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Type: ecs::PhysicsMaterial
typedef ecs::PhysicsMaterial sp_physics_material_t;

// Type: ecs::PhysicsShape
typedef ecs::PhysicsShape sp_physics_shape_t;

typedef sp::HeapVector<ecs::PhysicsShape> sp_physics_shape_vector_t;
SP_EXPORT size_t sp_physics_shape_vector_get_size(const sp_physics_shape_vector_t *v);
SP_EXPORT const sp_physics_shape_t *sp_physics_shape_vector_get_const_data(const sp_physics_shape_vector_t *v);
SP_EXPORT sp_physics_shape_t *sp_physics_shape_vector_get_data(sp_physics_shape_vector_t *v);
SP_EXPORT sp_physics_shape_t *sp_physics_shape_vector_resize(sp_physics_shape_vector_t *v, size_t new_size);

// Enum: ecs::PhysicsGroup
typedef ecs::PhysicsGroup sp_physics_group_t;
// Enum: ecs::PhysicsActorType
typedef ecs::PhysicsActorType sp_physics_actor_type_t;
// Component: physics
typedef ecs::Physics sp_ecs_physics_t;
const uint64_t SP_PHYSICS_INDEX = 6;
const uint64_t SP_ACCESS_PHYSICS = 2ull <<6;
SP_EXPORT sp_ecs_physics_t *sp_entity_set_physics(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_physics_t *sp_entity_get_physics(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_physics_t *sp_entity_get_const_physics(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_physics(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Type: sp::SceneRef
typedef sp::SceneRef sp_scene_ref_t;

// Component: active_scene
typedef ecs::ActiveScene sp_ecs_active_scene_t;
const uint64_t SP_ACTIVE_SCENE_INDEX = 7;
const uint64_t SP_ACCESS_ACTIVE_SCENE = 2ull <<7;
SP_EXPORT sp_ecs_active_scene_t *sp_ecs_set_active_scene(tecs_lock_t *dynLockPtr);
SP_EXPORT sp_ecs_active_scene_t *sp_ecs_get_active_scene(tecs_lock_t *dynLockPtr);
SP_EXPORT const sp_ecs_active_scene_t *sp_ecs_get_const_active_scene(tecs_lock_t *dynLockPtr);
SP_EXPORT void sp_ecs_unset_active_scene(tecs_lock_t *dynLockPtr);

// Type: ecs::AnimationState
typedef ecs::AnimationState sp_animation_state_t;

typedef sp::HeapVector<ecs::AnimationState> sp_animation_state_vector_t;
SP_EXPORT size_t sp_animation_state_vector_get_size(const sp_animation_state_vector_t *v);
SP_EXPORT const sp_animation_state_t *sp_animation_state_vector_get_const_data(const sp_animation_state_vector_t *v);
SP_EXPORT sp_animation_state_t *sp_animation_state_vector_get_data(sp_animation_state_vector_t *v);
SP_EXPORT sp_animation_state_t *sp_animation_state_vector_resize(sp_animation_state_vector_t *v, size_t new_size);

// Enum: ecs::InterpolationMode
typedef ecs::InterpolationMode sp_interpolation_mode_t;
// Component: animation
typedef ecs::Animation sp_ecs_animation_t;
const uint64_t SP_ANIMATION_INDEX = 8;
const uint64_t SP_ACCESS_ANIMATION = 2ull <<8;
SP_EXPORT sp_ecs_animation_t *sp_entity_set_animation(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_animation_t *sp_entity_get_animation(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_animation_t *sp_entity_get_const_animation(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_animation(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Enum: ecs::SoundType
typedef ecs::SoundType sp_sound_type_t;
// Type: ecs::Sound
typedef ecs::Sound sp_sound_t;

typedef sp::HeapVector<ecs::Sound> sp_sound_vector_t;
SP_EXPORT size_t sp_sound_vector_get_size(const sp_sound_vector_t *v);
SP_EXPORT const sp_sound_t *sp_sound_vector_get_const_data(const sp_sound_vector_t *v);
SP_EXPORT sp_sound_t *sp_sound_vector_get_data(sp_sound_vector_t *v);
SP_EXPORT sp_sound_t *sp_sound_vector_resize(sp_sound_vector_t *v, size_t new_size);

// Component: audio
typedef ecs::Audio sp_ecs_audio_t;
const uint64_t SP_AUDIO_INDEX = 9;
const uint64_t SP_ACCESS_AUDIO = 2ull <<9;
SP_EXPORT sp_ecs_audio_t *sp_entity_set_audio(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_audio_t *sp_entity_get_audio(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_audio_t *sp_entity_get_const_audio(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_audio(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: character_controller
typedef ecs::CharacterController sp_ecs_character_controller_t;
const uint64_t SP_CHARACTER_CONTROLLER_INDEX = 10;
const uint64_t SP_ACCESS_CHARACTER_CONTROLLER = 2ull <<10;
SP_EXPORT sp_ecs_character_controller_t *sp_entity_set_character_controller(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_character_controller_t *sp_entity_get_character_controller(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_character_controller_t *sp_entity_get_const_character_controller(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_character_controller(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: focus_lock
typedef ecs::FocusLock sp_ecs_focus_lock_t;
const uint64_t SP_FOCUS_LOCK_INDEX = 11;
const uint64_t SP_ACCESS_FOCUS_LOCK = 2ull <<11;
SP_EXPORT sp_ecs_focus_lock_t *sp_ecs_set_focus_lock(tecs_lock_t *dynLockPtr);
SP_EXPORT sp_ecs_focus_lock_t *sp_ecs_get_focus_lock(tecs_lock_t *dynLockPtr);
SP_EXPORT const sp_ecs_focus_lock_t *sp_ecs_get_const_focus_lock(tecs_lock_t *dynLockPtr);
SP_EXPORT void sp_ecs_unset_focus_lock(tecs_lock_t *dynLockPtr);
// Enum: ecs::FocusLayer
typedef ecs::FocusLayer sp_focus_layer_t;
SP_EXPORT bool sp_ecs_focus_lock_acquire_focus(sp_ecs_focus_lock_t *self, sp_focus_layer_t layer);
SP_EXPORT void sp_ecs_focus_lock_release_focus(sp_ecs_focus_lock_t *self, sp_focus_layer_t layer);
SP_EXPORT bool sp_ecs_focus_lock_has_primary_focus(const sp_ecs_focus_lock_t *self, sp_focus_layer_t layer);
SP_EXPORT bool sp_ecs_focus_lock_has_focus(const sp_ecs_focus_lock_t *self, sp_focus_layer_t layer);
SP_EXPORT void sp_ecs_focus_lock_primary_focus(const sp_ecs_focus_lock_t *self, sp_focus_layer_t *result);

// Enum: ecs::GuiLayoutAnchor
typedef ecs::GuiLayoutAnchor sp_gui_layout_anchor_t;
typedef glm::ivec2 ivec2_t;
// Component: gui_element
typedef ecs::GuiElement sp_ecs_gui_element_t;
const uint64_t SP_GUI_ELEMENT_INDEX = 12;
const uint64_t SP_ACCESS_GUI_ELEMENT = 2ull <<12;
SP_EXPORT sp_ecs_gui_element_t *sp_entity_set_gui_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_gui_element_t *sp_entity_get_gui_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_gui_element_t *sp_entity_get_const_gui_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_gui_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);

typedef sp::color_t sp_color_t;
// Component: laser_emitter
typedef ecs::LaserEmitter sp_ecs_laser_emitter_t;
const uint64_t SP_LASER_EMITTER_INDEX = 13;
const uint64_t SP_ACCESS_LASER_EMITTER = 2ull <<13;
SP_EXPORT sp_ecs_laser_emitter_t *sp_entity_set_laser_emitter(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_laser_emitter_t *sp_entity_get_laser_emitter(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_laser_emitter_t *sp_entity_get_const_laser_emitter(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_laser_emitter(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: laser_line
typedef ecs::LaserLine sp_ecs_laser_line_t;
const uint64_t SP_LASER_LINE_INDEX = 14;
const uint64_t SP_ACCESS_LASER_LINE = 2ull <<14;
SP_EXPORT sp_ecs_laser_line_t *sp_entity_set_laser_line(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_laser_line_t *sp_entity_get_laser_line(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_laser_line_t *sp_entity_get_const_laser_line(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_laser_line(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: laser_sensor
typedef ecs::LaserSensor sp_ecs_laser_sensor_t;
const uint64_t SP_LASER_SENSOR_INDEX = 15;
const uint64_t SP_ACCESS_LASER_SENSOR = 2ull <<15;
SP_EXPORT sp_ecs_laser_sensor_t *sp_entity_set_laser_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_laser_sensor_t *sp_entity_get_laser_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_laser_sensor_t *sp_entity_get_const_laser_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_laser_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);

typedef sp::angle_t sp_angle_t;
// Component: light
typedef ecs::Light sp_ecs_light_t;
const uint64_t SP_LIGHT_INDEX = 16;
const uint64_t SP_ACCESS_LIGHT = 2ull <<16;
SP_EXPORT sp_ecs_light_t *sp_entity_set_light(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_light_t *sp_entity_get_light(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_light_t *sp_entity_get_const_light(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_light(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: light_sensor
typedef ecs::LightSensor sp_ecs_light_sensor_t;
const uint64_t SP_LIGHT_SENSOR_INDEX = 17;
const uint64_t SP_ACCESS_LIGHT_SENSOR = 2ull <<17;
SP_EXPORT sp_ecs_light_sensor_t *sp_entity_set_light_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_light_sensor_t *sp_entity_get_light_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_light_sensor_t *sp_entity_get_const_light_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_light_sensor(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: optic
typedef ecs::OpticalElement sp_ecs_optical_element_t;
const uint64_t SP_OPTICAL_ELEMENT_INDEX = 18;
const uint64_t SP_ACCESS_OPTICAL_ELEMENT = 2ull <<18;
SP_EXPORT sp_ecs_optical_element_t *sp_entity_set_optical_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_optical_element_t *sp_entity_get_optical_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_optical_element_t *sp_entity_get_const_optical_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_optical_element(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Enum: ecs::PhysicsJointType
typedef ecs::PhysicsJointType sp_physics_joint_type_t;
// Type: ecs::PhysicsJoint
typedef ecs::PhysicsJoint sp_physics_joint_t;

typedef sp::HeapVector<ecs::PhysicsJoint> sp_physics_joint_vector_t;
SP_EXPORT size_t sp_physics_joint_vector_get_size(const sp_physics_joint_vector_t *v);
SP_EXPORT const sp_physics_joint_t *sp_physics_joint_vector_get_const_data(const sp_physics_joint_vector_t *v);
SP_EXPORT sp_physics_joint_t *sp_physics_joint_vector_get_data(sp_physics_joint_vector_t *v);
SP_EXPORT sp_physics_joint_t *sp_physics_joint_vector_resize(sp_physics_joint_vector_t *v, size_t new_size);

// Component: physics_joints
typedef ecs::PhysicsJoints sp_ecs_physics_joints_t;
const uint64_t SP_PHYSICS_JOINTS_INDEX = 19;
const uint64_t SP_ACCESS_PHYSICS_JOINTS = 2ull <<19;
SP_EXPORT sp_ecs_physics_joints_t *sp_entity_set_physics_joints(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_physics_joints_t *sp_entity_get_physics_joints(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_physics_joints_t *sp_entity_get_const_physics_joints(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_physics_joints(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: physics_query
typedef ecs::PhysicsQuery sp_ecs_physics_query_t;
const uint64_t SP_PHYSICS_QUERY_INDEX = 20;
const uint64_t SP_ACCESS_PHYSICS_QUERY = 2ull <<20;
SP_EXPORT sp_ecs_physics_query_t *sp_entity_set_physics_query(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_physics_query_t *sp_entity_get_physics_query(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_physics_query_t *sp_entity_get_const_physics_query(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_physics_query(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Type: ecs::SignalExpression
typedef ecs::SignalExpression sp_signal_expression_t;

typedef sp::HeapVector<ecs::EntityRef> sp_entity_ref_vector_t;
SP_EXPORT size_t sp_entity_ref_vector_get_size(const sp_entity_ref_vector_t *v);
SP_EXPORT const sp_entity_ref_t *sp_entity_ref_vector_get_const_data(const sp_entity_ref_vector_t *v);
SP_EXPORT sp_entity_ref_t *sp_entity_ref_vector_get_data(sp_entity_ref_vector_t *v);
SP_EXPORT sp_entity_ref_t *sp_entity_ref_vector_resize(sp_entity_ref_vector_t *v, size_t new_size);

// Component: render_output
typedef ecs::RenderOutput sp_ecs_render_output_t;
const uint64_t SP_RENDER_OUTPUT_INDEX = 21;
const uint64_t SP_ACCESS_RENDER_OUTPUT = 2ull <<21;
SP_EXPORT sp_ecs_render_output_t *sp_entity_set_render_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_render_output_t *sp_entity_get_render_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_render_output_t *sp_entity_get_const_render_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_render_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);

typedef sp::HeapVector<ecs::SignalExpression> sp_signal_expression_vector_t;
SP_EXPORT size_t sp_signal_expression_vector_get_size(const sp_signal_expression_vector_t *v);
SP_EXPORT const sp_signal_expression_t *sp_signal_expression_vector_get_const_data(const sp_signal_expression_vector_t *v);
SP_EXPORT sp_signal_expression_t *sp_signal_expression_vector_get_data(sp_signal_expression_vector_t *v);
SP_EXPORT sp_signal_expression_t *sp_signal_expression_vector_resize(sp_signal_expression_vector_t *v, size_t new_size);

typedef robin_hood::unordered_node_map<sp::InlineString<63>, sp::HeapVector<ecs::SignalExpression>> sp_string_63_signal_expression_vector_map_t;
// Component: scene_connection
typedef ecs::SceneConnection sp_ecs_scene_connection_t;
const uint64_t SP_SCENE_CONNECTION_INDEX = 22;
const uint64_t SP_ACCESS_SCENE_CONNECTION = 2ull <<22;
SP_EXPORT sp_ecs_scene_connection_t *sp_entity_set_scene_connection(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_scene_connection_t *sp_entity_get_scene_connection(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_scene_connection_t *sp_entity_get_const_scene_connection(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_scene_connection(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: screen
typedef ecs::Screen sp_ecs_screen_t;
const uint64_t SP_SCREEN_INDEX = 23;
const uint64_t SP_ACCESS_SCREEN = 2ull <<23;
SP_EXPORT sp_ecs_screen_t *sp_entity_set_screen(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_screen_t *sp_entity_get_screen(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_screen_t *sp_entity_get_const_screen(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_screen(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Enum: ecs::TriggerShape
typedef ecs::TriggerShape sp_trigger_shape_t;
// Component: trigger_area
typedef ecs::TriggerArea sp_ecs_trigger_area_t;
const uint64_t SP_TRIGGER_AREA_INDEX = 24;
const uint64_t SP_ACCESS_TRIGGER_AREA = 2ull <<24;
SP_EXPORT sp_ecs_trigger_area_t *sp_entity_set_trigger_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_trigger_area_t *sp_entity_get_trigger_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_trigger_area_t *sp_entity_get_const_trigger_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_trigger_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: trigger_group
typedef ecs::TriggerGroup sp_ecs_trigger_group_t;
const uint64_t SP_TRIGGER_GROUP_INDEX = 25;
const uint64_t SP_ACCESS_TRIGGER_GROUP = 2ull <<25;
SP_EXPORT sp_ecs_trigger_group_t *sp_entity_set_trigger_group(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_trigger_group_t *sp_entity_get_trigger_group(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_trigger_group_t *sp_entity_get_const_trigger_group(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_trigger_group(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: view
typedef ecs::View sp_ecs_view_t;
const uint64_t SP_VIEW_INDEX = 26;
const uint64_t SP_ACCESS_VIEW = 2ull <<26;
SP_EXPORT sp_ecs_view_t *sp_entity_set_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_view_t *sp_entity_get_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_view_t *sp_entity_get_const_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);

typedef glm::ivec3 ivec3_t;
// Component: voxel_area
typedef ecs::VoxelArea sp_ecs_voxel_area_t;
const uint64_t SP_VOXEL_AREA_INDEX = 27;
const uint64_t SP_ACCESS_VOXEL_AREA = 2ull <<27;
SP_EXPORT sp_ecs_voxel_area_t *sp_entity_set_voxel_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_voxel_area_t *sp_entity_get_voxel_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_voxel_area_t *sp_entity_get_const_voxel_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_voxel_area(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Enum: ecs::XrEye
typedef ecs::XrEye sp_xr_eye_t;
// Component: xr_view
typedef ecs::XrView sp_ecs_xr_view_t;
const uint64_t SP_XR_VIEW_INDEX = 28;
const uint64_t SP_ACCESS_XR_VIEW = 2ull <<28;
SP_EXPORT sp_ecs_xr_view_t *sp_entity_set_xr_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_xr_view_t *sp_entity_get_xr_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_xr_view_t *sp_entity_get_const_xr_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_xr_view(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: event_input
typedef ecs::EventInput sp_ecs_event_input_t;
const uint64_t SP_EVENT_INPUT_INDEX = 29;
const uint64_t SP_ACCESS_EVENT_INPUT = 2ull <<29;
SP_EXPORT sp_ecs_event_input_t *sp_entity_set_event_input(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_event_input_t *sp_entity_get_event_input(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_event_input_t *sp_entity_get_const_event_input(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_event_input(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Type: ecs::EventDest
typedef ecs::EventDest sp_event_dest_t;

typedef sp::HeapVector<ecs::EventDest> sp_event_dest_vector_t;
SP_EXPORT size_t sp_event_dest_vector_get_size(const sp_event_dest_vector_t *v);
SP_EXPORT const sp_event_dest_t *sp_event_dest_vector_get_const_data(const sp_event_dest_vector_t *v);
SP_EXPORT sp_event_dest_t *sp_event_dest_vector_get_data(sp_event_dest_vector_t *v);
SP_EXPORT sp_event_dest_t *sp_event_dest_vector_resize(sp_event_dest_vector_t *v, size_t new_size);

typedef std::optional<ecs::SignalExpression> sp_optional_signal_expression_t;
// Enum: ecs::EventDataType
typedef ecs::EventDataType sp_event_data_type_t;
typedef glm::vec4 vec4_t;
// Type: ecs::NamedEntity
typedef ecs::NamedEntity sp_named_entity_t;
SP_EXPORT void sp_named_entity_name(const sp_named_entity_t *self, sp_ecs_name_t *result);
SP_EXPORT tecs_entity_t sp_named_entity_get(const sp_named_entity_t *self, const tecs_lock_t * lock);
SP_EXPORT bool sp_named_entity_is_valid(const sp_named_entity_t *self);
SP_EXPORT void sp_named_entity_find(const char * ent, const sp_ecs_name_t * arg1, sp_named_entity_t *result);
SP_EXPORT void sp_named_entity_lookup(tecs_entity_t name, sp_named_entity_t *result);
SP_EXPORT void sp_named_entity_clear(sp_named_entity_t *self);

typedef ecs::EventBytes event_bytes_t;
// Type: ecs::EventData
typedef ecs::EventData sp_event_data_t;

typedef std::optional<ecs::EventData> sp_optional_event_data_t;
// Type: ecs::EventBindingActions
typedef ecs::EventBindingActions sp_event_binding_actions_t;

// Type: ecs::EventBinding
typedef ecs::EventBinding sp_event_binding_t;

typedef sp::HeapVector<ecs::EventBinding> sp_event_binding_vector_t;
SP_EXPORT size_t sp_event_binding_vector_get_size(const sp_event_binding_vector_t *v);
SP_EXPORT const sp_event_binding_t *sp_event_binding_vector_get_const_data(const sp_event_binding_vector_t *v);
SP_EXPORT sp_event_binding_t *sp_event_binding_vector_get_data(sp_event_binding_vector_t *v);
SP_EXPORT sp_event_binding_t *sp_event_binding_vector_resize(sp_event_binding_vector_t *v, size_t new_size);

typedef robin_hood::unordered_node_map<ecs::EventName, sp::HeapVector<ecs::EventBinding>> sp_event_name_event_binding_vector_map_t;
// Component: event_bindings
typedef ecs::EventBindings sp_ecs_event_bindings_t;
const uint64_t SP_EVENT_BINDINGS_INDEX = 30;
const uint64_t SP_ACCESS_EVENT_BINDINGS = 2ull <<30;
SP_EXPORT sp_ecs_event_bindings_t *sp_entity_set_event_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_event_bindings_t *sp_entity_get_event_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_event_bindings_t *sp_entity_get_const_event_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_event_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Component: signals
typedef ecs::Signals sp_ecs_signals_t;
const uint64_t SP_SIGNALS_INDEX = 31;
const uint64_t SP_ACCESS_SIGNALS = 2ull <<31;
SP_EXPORT sp_ecs_signals_t *sp_ecs_set_signals(tecs_lock_t *dynLockPtr);
SP_EXPORT sp_ecs_signals_t *sp_ecs_get_signals(tecs_lock_t *dynLockPtr);
SP_EXPORT const sp_ecs_signals_t *sp_ecs_get_const_signals(tecs_lock_t *dynLockPtr);
SP_EXPORT void sp_ecs_unset_signals(tecs_lock_t *dynLockPtr);

typedef robin_hood::unordered_node_map<ecs::EventName, double> sp_event_name_double_map_t;
// Component: signal_output
typedef ecs::SignalOutput sp_ecs_signal_output_t;
const uint64_t SP_SIGNAL_OUTPUT_INDEX = 32;
const uint64_t SP_ACCESS_SIGNAL_OUTPUT = 2ull <<32;
SP_EXPORT sp_ecs_signal_output_t *sp_entity_set_signal_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_signal_output_t *sp_entity_get_signal_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_signal_output_t *sp_entity_get_const_signal_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_signal_output(tecs_lock_t *dynLockPtr, sp_entity_t ent);

typedef robin_hood::unordered_node_map<ecs::EventName, ecs::SignalExpression> sp_event_name_signal_expression_map_t;
// Component: signal_bindings
typedef ecs::SignalBindings sp_ecs_signal_bindings_t;
const uint64_t SP_SIGNAL_BINDINGS_INDEX = 33;
const uint64_t SP_ACCESS_SIGNAL_BINDINGS = 2ull <<33;
SP_EXPORT sp_ecs_signal_bindings_t *sp_entity_set_signal_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_signal_bindings_t *sp_entity_get_signal_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_signal_bindings_t *sp_entity_get_const_signal_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_signal_bindings(tecs_lock_t *dynLockPtr, sp_entity_t ent);

// Type: ecs::ScriptInstance
typedef ecs::ScriptInstance sp_script_instance_t;
typedef sp::HeapString string_t;
SP_EXPORT void sp_string_set(string_t *str, const char *new_str);
SP_EXPORT int sp_string_compare(const string_t *str, const char *other_str);
SP_EXPORT size_t sp_string_get_size(const string_t *str);
SP_EXPORT const char *sp_string_get_c_str(const string_t *str);
SP_EXPORT char *sp_string_get_data(string_t *str);
SP_EXPORT char *sp_string_resize(string_t *str, size_t new_size, char fill_char);

// Enum: ecs::ScriptType
typedef ecs::ScriptType sp_script_type_t;
typedef sp::HeapVector<ecs::EventName> sp_event_name_vector_t;
SP_EXPORT size_t sp_event_name_vector_get_size(const sp_event_name_vector_t *v);
SP_EXPORT const event_name_t *sp_event_name_vector_get_const_data(const sp_event_name_vector_t *v);
SP_EXPORT event_name_t *sp_event_name_vector_get_data(sp_event_name_vector_t *v);
SP_EXPORT event_name_t *sp_event_name_vector_resize(sp_event_name_vector_t *v, size_t new_size);

// Type: ecs::ScriptDefinition
typedef ecs::ScriptDefinition sp_script_definition_t;

// Type: ecs::ScriptState
typedef ecs::ScriptState sp_script_state_t;
// Type: ecs::Event
typedef ecs::Event sp_event_t;
SP_EXPORT uint64_t sp_event_send(const tecs_lock_t * lock, tecs_entity_t target, const sp_event_t * event);
SP_EXPORT uint64_t sp_event_send_ref(const tecs_lock_t * lock, const sp_entity_ref_t * target, const sp_event_t * event);

SP_EXPORT sp_event_t * sp_script_state_poll_event(sp_script_state_t *self, const tecs_lock_t * lock);

SP_EXPORT sp_script_state_t * sp_script_instance_get_state(const sp_script_instance_t *self);

typedef sp::HeapVector<ecs::ScriptInstance> sp_script_instance_vector_t;
SP_EXPORT size_t sp_script_instance_vector_get_size(const sp_script_instance_vector_t *v);
SP_EXPORT const sp_script_instance_t *sp_script_instance_vector_get_const_data(const sp_script_instance_vector_t *v);
SP_EXPORT sp_script_instance_t *sp_script_instance_vector_get_data(sp_script_instance_vector_t *v);
SP_EXPORT sp_script_instance_t *sp_script_instance_vector_resize(sp_script_instance_vector_t *v, size_t new_size);

// Component: Scripts
typedef ecs::Scripts sp_ecs_scripts_t;
const uint64_t SP_SCRIPTS_INDEX = 34;
const uint64_t SP_ACCESS_SCRIPTS = 2ull <<34;
SP_EXPORT sp_ecs_scripts_t *sp_entity_set_scripts(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT sp_ecs_scripts_t *sp_entity_get_scripts(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT const sp_ecs_scripts_t *sp_entity_get_const_scripts(tecs_lock_t *dynLockPtr, sp_entity_t ent);
SP_EXPORT void sp_entity_unset_scripts(tecs_lock_t *dynLockPtr, sp_entity_t ent);

typedef glm::dvec2 dvec2_t;
typedef glm::dvec3 dvec3_t;
typedef glm::dvec4 dvec4_t;
typedef glm::ivec4 ivec4_t;
typedef glm::uvec2 uvec2_t;
typedef glm::uvec3 uvec3_t;
typedef glm::uvec4 uvec4_t;
// Type: ecs::TypeInfo
typedef ecs::TypeInfo sp_type_info_t;

// Enum: ecs::FieldAction
typedef ecs::FieldAction sp_field_action_t;
// Type: ecs::StructField
typedef ecs::StructField sp_struct_field_t;

typedef sp::HeapVector<ecs::StructField> sp_struct_field_vector_t;
SP_EXPORT size_t sp_struct_field_vector_get_size(const sp_struct_field_vector_t *v);
SP_EXPORT const sp_struct_field_t *sp_struct_field_vector_get_const_data(const sp_struct_field_vector_t *v);
SP_EXPORT sp_struct_field_t *sp_struct_field_vector_get_data(sp_struct_field_vector_t *v);
SP_EXPORT sp_struct_field_t *sp_struct_field_vector_resize(sp_struct_field_vector_t *v, size_t new_size);

// Type: sp::GuiDrawCommand
typedef sp::GuiDrawCommand sp_gui_draw_command_t;

typedef sp::HeapVector<sp::GuiDrawCommand> sp_gui_draw_command_vector_t;
SP_EXPORT size_t sp_gui_draw_command_vector_get_size(const sp_gui_draw_command_vector_t *v);
SP_EXPORT const sp_gui_draw_command_t *sp_gui_draw_command_vector_get_const_data(const sp_gui_draw_command_vector_t *v);
SP_EXPORT sp_gui_draw_command_t *sp_gui_draw_command_vector_get_data(sp_gui_draw_command_vector_t *v);
SP_EXPORT sp_gui_draw_command_t *sp_gui_draw_command_vector_resize(sp_gui_draw_command_vector_t *v, size_t new_size);

typedef sp::HeapVector<uint16_t> sp_uint16_vector_t;
SP_EXPORT size_t sp_uint16_vector_get_size(const sp_uint16_vector_t *v);
SP_EXPORT const uint16_t *sp_uint16_vector_get_const_data(const sp_uint16_vector_t *v);
SP_EXPORT uint16_t *sp_uint16_vector_get_data(sp_uint16_vector_t *v);
SP_EXPORT uint16_t *sp_uint16_vector_resize(sp_uint16_vector_t *v, size_t new_size);

// Type: sp::GuiDrawVertex
typedef sp::GuiDrawVertex sp_gui_draw_vertex_t;

typedef sp::HeapVector<sp::GuiDrawVertex> sp_gui_draw_vertex_vector_t;
SP_EXPORT size_t sp_gui_draw_vertex_vector_get_size(const sp_gui_draw_vertex_vector_t *v);
SP_EXPORT const sp_gui_draw_vertex_t *sp_gui_draw_vertex_vector_get_const_data(const sp_gui_draw_vertex_vector_t *v);
SP_EXPORT sp_gui_draw_vertex_t *sp_gui_draw_vertex_vector_get_data(sp_gui_draw_vertex_vector_t *v);
SP_EXPORT sp_gui_draw_vertex_t *sp_gui_draw_vertex_vector_resize(sp_gui_draw_vertex_vector_t *v, size_t new_size);

// Type: sp::GuiDrawData
typedef sp::GuiDrawData sp_gui_draw_data_t;

// Type: ecs::DynamicScriptDefinition
typedef ecs::DynamicScriptDefinition sp_dynamic_script_definition_t;

// Type: ecs::ScriptDefinitionBase
typedef ecs::ScriptDefinitionBase sp_script_definition_base_t;
SP_EXPORT void * sp_script_definition_base_access_mut(const sp_script_definition_base_t *self, sp_script_state_t * arg0);
SP_EXPORT const void * sp_script_definition_base_access(const sp_script_definition_base_t *self, const sp_script_state_t * arg0);

// Type: ecs::SignalRef
typedef ecs::SignalRef sp_signal_ref_t;
SP_EXPORT const sp_entity_ref_t * sp_signal_ref_get_entity(const sp_signal_ref_t *self);
SP_EXPORT const string_t * sp_signal_ref_get_signal_name(const sp_signal_ref_t *self);
SP_EXPORT void sp_signal_ref_string(const sp_signal_ref_t *self, string_t *result);
SP_EXPORT bool sp_signal_ref_is_valid(const sp_signal_ref_t *self);
SP_EXPORT double * sp_signal_ref_set_value(const sp_signal_ref_t *self, const tecs_lock_t * lock, double value);
SP_EXPORT bool sp_signal_ref_has_value(const sp_signal_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT void sp_signal_ref_clear_value(const sp_signal_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT const double * sp_signal_ref_get_value(const sp_signal_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT sp_signal_expression_t * sp_signal_ref_set_binding(const sp_signal_ref_t *self, const tecs_lock_t * lock, const sp_signal_expression_t * expr);
SP_EXPORT bool sp_signal_ref_has_binding(const sp_signal_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT void sp_signal_ref_clear_binding(const sp_signal_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT const sp_signal_expression_t * sp_signal_ref_get_binding(const sp_signal_ref_t *self, const tecs_lock_t * lock);
SP_EXPORT double sp_signal_ref_get_signal(const sp_signal_ref_t *self, const tecs_lock_t * lock, uint32_t depth);
SP_EXPORT void sp_signal_ref_empty(sp_signal_ref_t *result);
SP_EXPORT void sp_signal_ref_new(const sp_entity_ref_t * ent, const char * signal_name, sp_signal_ref_t *result);
SP_EXPORT void sp_signal_ref_copy(const sp_signal_ref_t * ref, sp_signal_ref_t *result);
SP_EXPORT void sp_signal_ref_lookup(const char * str, const sp_ecs_name_t * scope, sp_signal_ref_t *result);
SP_EXPORT void sp_signal_ref_clear(sp_signal_ref_t *self);

typedef sp::InlineString<2> string_2_t;
typedef sp::HeapVector<float> sp_float_vector_t;
SP_EXPORT size_t sp_float_vector_get_size(const sp_float_vector_t *v);
SP_EXPORT const float *sp_float_vector_get_const_data(const sp_float_vector_t *v);
SP_EXPORT float *sp_float_vector_get_data(sp_float_vector_t *v);
SP_EXPORT float *sp_float_vector_resize(sp_float_vector_t *v, size_t new_size);

typedef sp::HeapVector<glm::vec2> sp_vec2_vector_t;
SP_EXPORT size_t sp_vec2_vector_get_size(const sp_vec2_vector_t *v);
SP_EXPORT const vec2_t *sp_vec2_vector_get_const_data(const sp_vec2_vector_t *v);
SP_EXPORT vec2_t *sp_vec2_vector_get_data(sp_vec2_vector_t *v);
SP_EXPORT vec2_t *sp_vec2_vector_resize(sp_vec2_vector_t *v, size_t new_size);

typedef sp::HeapVector<sp::HeapString> sp_string_vector_t;
SP_EXPORT size_t sp_string_vector_get_size(const sp_string_vector_t *v);
SP_EXPORT const string_t *sp_string_vector_get_const_data(const sp_string_vector_t *v);
SP_EXPORT string_t *sp_string_vector_get_data(sp_string_vector_t *v);
SP_EXPORT string_t *sp_string_vector_resize(sp_string_vector_t *v, size_t new_size);

typedef sp::HeapVector<ecs::EventString> sp_event_string_vector_t;
SP_EXPORT size_t sp_event_string_vector_get_size(const sp_event_string_vector_t *v);
SP_EXPORT const event_string_t *sp_event_string_vector_get_const_data(const sp_event_string_vector_t *v);
SP_EXPORT event_string_t *sp_event_string_vector_get_data(sp_event_string_vector_t *v);
SP_EXPORT event_string_t *sp_event_string_vector_resize(sp_event_string_vector_t *v, size_t new_size);

typedef std::pair<ecs::EntityRef, ecs::EntityRef> sp_entity_ref_pair_t;
typedef sp::HeapVector<std::pair<ecs::EntityRef, ecs::EntityRef>> sp_entity_ref_pair_vector_t;
SP_EXPORT size_t sp_entity_ref_pair_vector_get_size(const sp_entity_ref_pair_vector_t *v);
SP_EXPORT const sp_entity_ref_pair_t *sp_entity_ref_pair_vector_get_const_data(const sp_entity_ref_pair_vector_t *v);
SP_EXPORT sp_entity_ref_pair_t *sp_entity_ref_pair_vector_get_data(sp_entity_ref_pair_vector_t *v);
SP_EXPORT sp_entity_ref_pair_t *sp_entity_ref_pair_vector_resize(sp_entity_ref_pair_vector_t *v, size_t new_size);

typedef std::optional<double> sp_optional_double_t;
typedef std::optional<ecs::PhysicsActorType> sp_optional_physics_actor_type_t;
typedef robin_hood::unordered_node_map<ecs::EventName, ecs::EventName> sp_event_name_event_name_map_t;
typedef robin_hood::unordered_node_map<sp::HeapString, ecs::SignalExpression> sp_string_signal_expression_map_t;
typedef robin_hood::unordered_node_map<ecs::EventName, ecs::PhysicsJoint> sp_event_name_physics_joint_map_t;
// Enum: sp::ScenePriority
typedef sp::ScenePriority sp_scene_priority_t;

#pragma pack(pop)
} // extern "C"
#endif
