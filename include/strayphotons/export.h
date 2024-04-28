/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#ifndef SP_EXPORT
    #ifdef SP_SHARED_INTERNAL
        /* We are building this library */
        #ifdef _WIN32
            #define SP_EXPORT __declspec(dllexport)
        #else
            #define SP_EXPORT __attribute__((__visibility__("default")))
        #endif
    #else
        /* We are using this library */
        #ifdef _WIN32
            #define SP_EXPORT __declspec(dllimport)
        #else
            #define SP_EXPORT
        #endif
    #endif
#endif
