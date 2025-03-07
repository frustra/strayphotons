/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

struct DrawParams {
    uint16_t baseColorTexID;
    uint16_t metallicRoughnessTexID;
    uint16_t opticID;
    float16_t emissiveScale;
    uint baseColorTint; // RGBA packed u8
};
