/*
 * THIS FILE IS AUTO-GENERATED -- DO NOT EDIT
 * See src/codegen/gen_main.cc to modify
 */

#pragma once

#include "Tecs_abi_entity.h"
#include "Tecs_abi_entity_view.h"
#include "Tecs_abi_export.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static_assert(sizeof(bool) == 1, "Unexpected bool size");
#include "strayphotons/components_gen.h"

typedef void tecs_lock_t;
typedef uint64_t tecs_entity_t;

TECS_EXPORT bool Tecs_lock_is_write_name_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_name_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_name(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_name(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_scene_info_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_scene_info_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_scene_info(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_scene_info(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_scene_properties_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_scene_properties_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_scene_properties(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_scene_properties(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_transform_snapshot_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_transform_snapshot_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_transform_snapshot(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_transform_snapshot(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_transform_tree_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_transform_tree_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_transform_tree(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_transform_tree(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_renderable_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_renderable_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_renderable(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_renderable(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_physics_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_physics_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_physics(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_physics(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_active_scene_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_active_scene_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT bool Tecs_has_active_scene(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_had_active_scene(tecs_lock_t *dynLockPtr);
TECS_EXPORT const void *Tecs_const_get_active_scene(tecs_lock_t *dynLockPtr);
TECS_EXPORT void *Tecs_get_active_scene(tecs_lock_t *dynLockPtr);
TECS_EXPORT const void *Tecs_get_previous_active_scene(tecs_lock_t *dynLockPtr);
TECS_EXPORT void *Tecs_set_active_scene(tecs_lock_t *dynLockPtr, const void *value);
TECS_EXPORT void Tecs_unset_active_scene(tecs_lock_t *dynLockPtr);

TECS_EXPORT bool Tecs_lock_is_write_animation_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_animation_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_animation(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_animation(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_audio_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_audio_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_audio(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_audio(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_character_controller_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_character_controller_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_character_controller(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_character_controller(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_focus_lock_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_focus_lock_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT bool Tecs_has_focus_lock(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_had_focus_lock(tecs_lock_t *dynLockPtr);
TECS_EXPORT const void *Tecs_const_get_focus_lock(tecs_lock_t *dynLockPtr);
TECS_EXPORT void *Tecs_get_focus_lock(tecs_lock_t *dynLockPtr);
TECS_EXPORT const void *Tecs_get_previous_focus_lock(tecs_lock_t *dynLockPtr);
TECS_EXPORT void *Tecs_set_focus_lock(tecs_lock_t *dynLockPtr, const void *value);
TECS_EXPORT void Tecs_unset_focus_lock(tecs_lock_t *dynLockPtr);

TECS_EXPORT bool Tecs_lock_is_write_gui_element_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_gui_element_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_gui_element(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_gui_element(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_laser_emitter_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_laser_emitter_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_laser_emitter(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_laser_emitter(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_laser_line_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_laser_line_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_laser_line(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_laser_line(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_laser_sensor_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_laser_sensor_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_laser_sensor(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_laser_sensor(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_light_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_light_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_light(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_light(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_light_sensor_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_light_sensor_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_light_sensor(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_light_sensor(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_optical_element_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_optical_element_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_optical_element(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_optical_element(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_physics_joints_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_physics_joints_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_physics_joints(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_physics_joints(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_physics_query_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_physics_query_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_physics_query(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_physics_query(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_render_output_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_render_output_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_render_output(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_render_output(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_scene_connection_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_scene_connection_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_scene_connection(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_scene_connection(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_screen_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_screen_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_screen(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_screen(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_trigger_area_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_trigger_area_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_trigger_area(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_trigger_area(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_trigger_group_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_trigger_group_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_trigger_group(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_trigger_group(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_view_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_view_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_view(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_view(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_voxel_area_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_voxel_area_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_voxel_area(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_voxel_area(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_xr_view_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_xr_view_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_xr_view(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_xr_view(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_event_input_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_event_input_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_event_input(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_event_input(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_event_bindings_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_event_bindings_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_event_bindings(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_event_bindings(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_component31_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_component31_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT bool Tecs_has_component31(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_had_component31(tecs_lock_t *dynLockPtr);
TECS_EXPORT const void *Tecs_const_get_component31(tecs_lock_t *dynLockPtr);
TECS_EXPORT void *Tecs_get_component31(tecs_lock_t *dynLockPtr);
TECS_EXPORT const void *Tecs_get_previous_component31(tecs_lock_t *dynLockPtr);
TECS_EXPORT void *Tecs_set_component31(tecs_lock_t *dynLockPtr, const void *value);
TECS_EXPORT void Tecs_unset_component31(tecs_lock_t *dynLockPtr);

TECS_EXPORT bool Tecs_lock_is_write_signal_output_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_signal_output_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_signal_output(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_signal_output(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_signal_bindings_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_signal_bindings_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_signal_bindings(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_signal_bindings(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT bool Tecs_lock_is_write_scripts_allowed(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_lock_is_read_scripts_allowed(tecs_lock_t *dynLockPtr);

TECS_EXPORT uint64_t Tecs_previous_entities_with_scripts(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);
TECS_EXPORT uint64_t Tecs_entities_with_scripts(tecs_lock_t *dynLockPtr, tecs_entity_view_t *output);

TECS_EXPORT const sp_ecs_name_t *Tecs_get_entity_name_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_name_t *Tecs_get_previous_entity_name_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_name(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_name(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_name_t *Tecs_entity_const_get_name(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_name_t *Tecs_entity_get_name(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_name_t *Tecs_entity_get_previous_name(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_name_t *Tecs_entity_set_name(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_name_t *value);
TECS_EXPORT void Tecs_entity_unset_name(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_scene_info_t *Tecs_get_entity_scene_info_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_scene_info_t *Tecs_get_previous_entity_scene_info_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_scene_info(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_scene_info(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_scene_info_t *Tecs_entity_const_get_scene_info(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_scene_info_t *Tecs_entity_get_scene_info(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_scene_info_t *Tecs_entity_get_previous_scene_info(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_scene_info_t *Tecs_entity_set_scene_info(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_scene_info_t *value);
TECS_EXPORT void Tecs_entity_unset_scene_info(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_scene_properties_t *Tecs_get_entity_scene_properties_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_scene_properties_t *Tecs_get_previous_entity_scene_properties_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_scene_properties(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_scene_properties(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_scene_properties_t *Tecs_entity_const_get_scene_properties(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_scene_properties_t *Tecs_entity_get_scene_properties(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_scene_properties_t *Tecs_entity_get_previous_scene_properties(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_scene_properties_t *Tecs_entity_set_scene_properties(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_scene_properties_t *value);
TECS_EXPORT void Tecs_entity_unset_scene_properties(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_transform_snapshot_t *Tecs_get_entity_transform_snapshot_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_transform_snapshot_t *Tecs_get_previous_entity_transform_snapshot_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_transform_snapshot(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_transform_snapshot(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_transform_snapshot_t *Tecs_entity_const_get_transform_snapshot(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_transform_snapshot_t *Tecs_entity_get_transform_snapshot(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_transform_snapshot_t *Tecs_entity_get_previous_transform_snapshot(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_transform_snapshot_t *Tecs_entity_set_transform_snapshot(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_transform_snapshot_t *value);
TECS_EXPORT void Tecs_entity_unset_transform_snapshot(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_transform_tree_t *Tecs_get_entity_transform_tree_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_transform_tree_t *Tecs_get_previous_entity_transform_tree_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_transform_tree(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_transform_tree(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_transform_tree_t *Tecs_entity_const_get_transform_tree(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_transform_tree_t *Tecs_entity_get_transform_tree(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_transform_tree_t *Tecs_entity_get_previous_transform_tree(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_transform_tree_t *Tecs_entity_set_transform_tree(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_transform_tree_t *value);
TECS_EXPORT void Tecs_entity_unset_transform_tree(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_renderable_t *Tecs_get_entity_renderable_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_renderable_t *Tecs_get_previous_entity_renderable_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_renderable(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_renderable(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_renderable_t *Tecs_entity_const_get_renderable(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_renderable_t *Tecs_entity_get_renderable(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_renderable_t *Tecs_entity_get_previous_renderable(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_renderable_t *Tecs_entity_set_renderable(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_renderable_t *value);
TECS_EXPORT void Tecs_entity_unset_renderable(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_physics_t *Tecs_get_entity_physics_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_physics_t *Tecs_get_previous_entity_physics_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_physics(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_physics(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_physics_t *Tecs_entity_const_get_physics(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_physics_t *Tecs_entity_get_physics(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_physics_t *Tecs_entity_get_previous_physics(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_physics_t *Tecs_entity_set_physics(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_physics_t *value);
TECS_EXPORT void Tecs_entity_unset_physics(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_animation_t *Tecs_get_entity_animation_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_animation_t *Tecs_get_previous_entity_animation_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_animation(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_animation(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_animation_t *Tecs_entity_const_get_animation(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_animation_t *Tecs_entity_get_animation(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_animation_t *Tecs_entity_get_previous_animation(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_animation_t *Tecs_entity_set_animation(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_animation_t *value);
TECS_EXPORT void Tecs_entity_unset_animation(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_audio_t *Tecs_get_entity_audio_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_audio_t *Tecs_get_previous_entity_audio_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_audio(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_audio(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_audio_t *Tecs_entity_const_get_audio(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_audio_t *Tecs_entity_get_audio(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_audio_t *Tecs_entity_get_previous_audio(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_audio_t *Tecs_entity_set_audio(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_audio_t *value);
TECS_EXPORT void Tecs_entity_unset_audio(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_character_controller_t *Tecs_get_entity_character_controller_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_character_controller_t *Tecs_get_previous_entity_character_controller_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_character_controller(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_character_controller(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_character_controller_t *Tecs_entity_const_get_character_controller(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_character_controller_t *Tecs_entity_get_character_controller(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_character_controller_t *Tecs_entity_get_previous_character_controller(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_character_controller_t *Tecs_entity_set_character_controller(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_character_controller_t *value);
TECS_EXPORT void Tecs_entity_unset_character_controller(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_gui_element_t *Tecs_get_entity_gui_element_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_gui_element_t *Tecs_get_previous_entity_gui_element_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_gui_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_gui_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_gui_element_t *Tecs_entity_const_get_gui_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_gui_element_t *Tecs_entity_get_gui_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_gui_element_t *Tecs_entity_get_previous_gui_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_gui_element_t *Tecs_entity_set_gui_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_gui_element_t *value);
TECS_EXPORT void Tecs_entity_unset_gui_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_laser_emitter_t *Tecs_get_entity_laser_emitter_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_laser_emitter_t *Tecs_get_previous_entity_laser_emitter_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_laser_emitter(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_laser_emitter(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_laser_emitter_t *Tecs_entity_const_get_laser_emitter(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_laser_emitter_t *Tecs_entity_get_laser_emitter(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_laser_emitter_t *Tecs_entity_get_previous_laser_emitter(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_laser_emitter_t *Tecs_entity_set_laser_emitter(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_laser_emitter_t *value);
TECS_EXPORT void Tecs_entity_unset_laser_emitter(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_laser_line_t *Tecs_get_entity_laser_line_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_laser_line_t *Tecs_get_previous_entity_laser_line_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_laser_line(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_laser_line(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_laser_line_t *Tecs_entity_const_get_laser_line(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_laser_line_t *Tecs_entity_get_laser_line(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_laser_line_t *Tecs_entity_get_previous_laser_line(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_laser_line_t *Tecs_entity_set_laser_line(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_laser_line_t *value);
TECS_EXPORT void Tecs_entity_unset_laser_line(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_laser_sensor_t *Tecs_get_entity_laser_sensor_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_laser_sensor_t *Tecs_get_previous_entity_laser_sensor_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_laser_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_laser_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_laser_sensor_t *Tecs_entity_const_get_laser_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_laser_sensor_t *Tecs_entity_get_laser_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_laser_sensor_t *Tecs_entity_get_previous_laser_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_laser_sensor_t *Tecs_entity_set_laser_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_laser_sensor_t *value);
TECS_EXPORT void Tecs_entity_unset_laser_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_light_t *Tecs_get_entity_light_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_light_t *Tecs_get_previous_entity_light_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_light(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_light(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_light_t *Tecs_entity_const_get_light(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_light_t *Tecs_entity_get_light(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_light_t *Tecs_entity_get_previous_light(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_light_t *Tecs_entity_set_light(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_light_t *value);
TECS_EXPORT void Tecs_entity_unset_light(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_light_sensor_t *Tecs_get_entity_light_sensor_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_light_sensor_t *Tecs_get_previous_entity_light_sensor_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_light_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_light_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_light_sensor_t *Tecs_entity_const_get_light_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_light_sensor_t *Tecs_entity_get_light_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_light_sensor_t *Tecs_entity_get_previous_light_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_light_sensor_t *Tecs_entity_set_light_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_light_sensor_t *value);
TECS_EXPORT void Tecs_entity_unset_light_sensor(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_optical_element_t *Tecs_get_entity_optical_element_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_optical_element_t *Tecs_get_previous_entity_optical_element_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_optical_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_optical_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_optical_element_t *Tecs_entity_const_get_optical_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_optical_element_t *Tecs_entity_get_optical_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_optical_element_t *Tecs_entity_get_previous_optical_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_optical_element_t *Tecs_entity_set_optical_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_optical_element_t *value);
TECS_EXPORT void Tecs_entity_unset_optical_element(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_physics_joints_t *Tecs_get_entity_physics_joints_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_physics_joints_t *Tecs_get_previous_entity_physics_joints_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_physics_joints(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_physics_joints(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_physics_joints_t *Tecs_entity_const_get_physics_joints(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_physics_joints_t *Tecs_entity_get_physics_joints(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_physics_joints_t *Tecs_entity_get_previous_physics_joints(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_physics_joints_t *Tecs_entity_set_physics_joints(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_physics_joints_t *value);
TECS_EXPORT void Tecs_entity_unset_physics_joints(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_physics_query_t *Tecs_get_entity_physics_query_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_physics_query_t *Tecs_get_previous_entity_physics_query_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_physics_query(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_physics_query(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_physics_query_t *Tecs_entity_const_get_physics_query(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_physics_query_t *Tecs_entity_get_physics_query(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_physics_query_t *Tecs_entity_get_previous_physics_query(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_physics_query_t *Tecs_entity_set_physics_query(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_physics_query_t *value);
TECS_EXPORT void Tecs_entity_unset_physics_query(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_render_output_t *Tecs_get_entity_render_output_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_render_output_t *Tecs_get_previous_entity_render_output_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_render_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_render_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_render_output_t *Tecs_entity_const_get_render_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_render_output_t *Tecs_entity_get_render_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_render_output_t *Tecs_entity_get_previous_render_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_render_output_t *Tecs_entity_set_render_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_render_output_t *value);
TECS_EXPORT void Tecs_entity_unset_render_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_scene_connection_t *Tecs_get_entity_scene_connection_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_scene_connection_t *Tecs_get_previous_entity_scene_connection_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_scene_connection(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_scene_connection(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_scene_connection_t *Tecs_entity_const_get_scene_connection(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_scene_connection_t *Tecs_entity_get_scene_connection(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_scene_connection_t *Tecs_entity_get_previous_scene_connection(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_scene_connection_t *Tecs_entity_set_scene_connection(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_scene_connection_t *value);
TECS_EXPORT void Tecs_entity_unset_scene_connection(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_screen_t *Tecs_get_entity_screen_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_screen_t *Tecs_get_previous_entity_screen_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_screen(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_screen(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_screen_t *Tecs_entity_const_get_screen(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_screen_t *Tecs_entity_get_screen(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_screen_t *Tecs_entity_get_previous_screen(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_screen_t *Tecs_entity_set_screen(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_screen_t *value);
TECS_EXPORT void Tecs_entity_unset_screen(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_trigger_area_t *Tecs_get_entity_trigger_area_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_trigger_area_t *Tecs_get_previous_entity_trigger_area_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_trigger_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_trigger_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_trigger_area_t *Tecs_entity_const_get_trigger_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_trigger_area_t *Tecs_entity_get_trigger_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_trigger_area_t *Tecs_entity_get_previous_trigger_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_trigger_area_t *Tecs_entity_set_trigger_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_trigger_area_t *value);
TECS_EXPORT void Tecs_entity_unset_trigger_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const unsigned char *Tecs_get_entity_trigger_group_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const unsigned char *Tecs_get_previous_entity_trigger_group_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_trigger_group(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_trigger_group(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const unsigned char *Tecs_entity_const_get_trigger_group(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT unsigned char *Tecs_entity_get_trigger_group(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const unsigned char *Tecs_entity_get_previous_trigger_group(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT unsigned char *Tecs_entity_set_trigger_group(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const unsigned char *value);
TECS_EXPORT void Tecs_entity_unset_trigger_group(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_view_t *Tecs_get_entity_view_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_view_t *Tecs_get_previous_entity_view_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_view_t *Tecs_entity_const_get_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_view_t *Tecs_entity_get_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_view_t *Tecs_entity_get_previous_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_view_t *Tecs_entity_set_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_view_t *value);
TECS_EXPORT void Tecs_entity_unset_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_voxel_area_t *Tecs_get_entity_voxel_area_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_voxel_area_t *Tecs_get_previous_entity_voxel_area_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_voxel_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_voxel_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_voxel_area_t *Tecs_entity_const_get_voxel_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_voxel_area_t *Tecs_entity_get_voxel_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_voxel_area_t *Tecs_entity_get_previous_voxel_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_voxel_area_t *Tecs_entity_set_voxel_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_voxel_area_t *value);
TECS_EXPORT void Tecs_entity_unset_voxel_area(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_xr_view_t *Tecs_get_entity_xr_view_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_xr_view_t *Tecs_get_previous_entity_xr_view_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_xr_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_xr_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_xr_view_t *Tecs_entity_const_get_xr_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_xr_view_t *Tecs_entity_get_xr_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_xr_view_t *Tecs_entity_get_previous_xr_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_xr_view_t *Tecs_entity_set_xr_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_xr_view_t *value);
TECS_EXPORT void Tecs_entity_unset_xr_view(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_event_input_t *Tecs_get_entity_event_input_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_event_input_t *Tecs_get_previous_entity_event_input_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_event_input(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_event_input(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_event_input_t *Tecs_entity_const_get_event_input(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_event_input_t *Tecs_entity_get_event_input(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_event_input_t *Tecs_entity_get_previous_event_input(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_event_input_t *Tecs_entity_set_event_input(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_event_input_t *value);
TECS_EXPORT void Tecs_entity_unset_event_input(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_event_bindings_t *Tecs_get_entity_event_bindings_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_event_bindings_t *Tecs_get_previous_entity_event_bindings_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_event_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_event_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_event_bindings_t *Tecs_entity_const_get_event_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_event_bindings_t *Tecs_entity_get_event_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_event_bindings_t *Tecs_entity_get_previous_event_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_event_bindings_t *Tecs_entity_set_event_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_event_bindings_t *value);
TECS_EXPORT void Tecs_entity_unset_event_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_signal_output_t *Tecs_get_entity_signal_output_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_signal_output_t *Tecs_get_previous_entity_signal_output_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_signal_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_signal_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_signal_output_t *Tecs_entity_const_get_signal_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_signal_output_t *Tecs_entity_get_signal_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_signal_output_t *Tecs_entity_get_previous_signal_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_signal_output_t *Tecs_entity_set_signal_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_signal_output_t *value);
TECS_EXPORT void Tecs_entity_unset_signal_output(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_signal_bindings_t *Tecs_get_entity_signal_bindings_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_signal_bindings_t *Tecs_get_previous_entity_signal_bindings_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_signal_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_signal_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_signal_bindings_t *Tecs_entity_const_get_signal_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_signal_bindings_t *Tecs_entity_get_signal_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_signal_bindings_t *Tecs_entity_get_previous_signal_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_signal_bindings_t *Tecs_entity_set_signal_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_signal_bindings_t *value);
TECS_EXPORT void Tecs_entity_unset_signal_bindings(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

TECS_EXPORT const sp_ecs_scripts_t *Tecs_get_entity_scripts_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT const sp_ecs_scripts_t *Tecs_get_previous_entity_scripts_storage(tecs_lock_t *dynLockPtr);
TECS_EXPORT bool Tecs_entity_has_scripts(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT bool Tecs_entity_had_scripts(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_scripts_t *Tecs_entity_const_get_scripts(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_scripts_t *Tecs_entity_get_scripts(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT const sp_ecs_scripts_t *Tecs_entity_get_previous_scripts(tecs_lock_t *dynLockPtr, tecs_entity_t entity);
TECS_EXPORT sp_ecs_scripts_t *Tecs_entity_set_scripts(tecs_lock_t *dynLockPtr, tecs_entity_t entity, const sp_ecs_scripts_t *value);
TECS_EXPORT void Tecs_entity_unset_scripts(tecs_lock_t *dynLockPtr, tecs_entity_t entity);

#ifdef __cplusplus
}
#endif
