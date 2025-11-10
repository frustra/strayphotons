/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <common/InlineString.hh>

struct ColumnMetadata {
    sp::InlineString<127> name;
    sp::InlineString<63> unit;
    uint32_t columnIndex;
    double min, max;
    size_t sampleRate;
};

struct ColumnRange : public std::array<float, (256 - (sizeof(size_t) * 3)) / sizeof(float)> {
    uint32_t columnIndex;
    size_t rangeStart, rangeEnd;
};
