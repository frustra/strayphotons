#
# Stray Photons - Copyright (C) 2025 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

add_library(doomgeneric SHARED
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/dummy.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/am_map.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/doomdef.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/doomstat.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/dstrings.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/d_event.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/d_items.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/d_iwad.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/d_loop.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/d_main.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/d_mode.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/d_net.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/f_finale.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/f_wipe.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/g_game.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/hu_lib.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/hu_stuff.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/info.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/i_cdmus.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/i_endoom.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/i_joystick.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/i_scale.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/i_sound.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/i_system.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/i_timer.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/memio.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/m_argv.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/m_bbox.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/m_cheat.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/m_config.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/m_controls.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/m_fixed.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/m_menu.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/m_misc.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/m_random.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_ceilng.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_doors.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_enemy.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_floor.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_inter.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_lights.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_map.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_maputl.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_mobj.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_plats.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_pspr.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_saveg.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_setup.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_sight.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_spec.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_switch.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_telept.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_tick.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/p_user.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/r_bsp.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/r_data.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/r_draw.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/r_main.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/r_plane.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/r_segs.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/r_sky.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/r_things.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/sha1.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/sounds.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/statdump.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/st_lib.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/st_stuff.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/s_sound.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/tables.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/v_video.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/wi_stuff.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/w_checksum.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/w_file.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/w_main.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/w_wad.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/z_zone.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/w_file_stdc.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/i_input.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/i_video.c
    ${PROJECT_ROOT_DIR}/ext/doomgeneric/doomgeneric/doomgeneric.c
    doomgeneric.cpp
)

target_include_directories(doomgeneric PUBLIC
    ${PROJECT_ROOT_DIR}/ext/doomgeneric
)
set_target_properties(doomgeneric PROPERTIES C_STANDARD 17)

target_link_libraries(doomgeneric PRIVATE ${PROJECT_LIB_NAME} ${PROJECT_COMMON_LIB})

if(WIN32)
    target_compile_definitions(doomgeneric PUBLIC _CRT_SECURE_NO_WARNINGS)
    add_custom_command(TARGET doomgeneric PRE_LINK
        COMMAND
            ./extra/remove_in_use.py plugins/doomgeneric dll pdb
        WORKING_DIRECTORY ${PROJECT_ROOT_DIR}
    )
endif()
