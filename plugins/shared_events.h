/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <common/InlineString.hh>
#include <cstdint>

struct ColumnMetadata {
    sp::InlineString<127> name;
    sp::InlineString<63> unit;
    uint32_t columnIndex;
    double min, max;
    size_t sampleRate;
    size_t firstTimestamp, lastTimestamp;
};

static_assert(sizeof(ColumnMetadata) <= 256, "ColumnMetadata is too large to fit in EventBytes");

struct MinMaxSample {
    float min, max;
};

struct ColumnRange : public std::array<MinMaxSample, (256 - (sizeof(uint32_t) * 4)) / sizeof(MinMaxSample)> {
    uint32_t columnIndex, sampleCount, sampleOffset, _padding;
};
static_assert(sizeof(ColumnRange) == 256, "ColumnRange size isn't matched to EventBytes");

struct SampleAverage {
    float sum = 0;
    uint32_t sampleCount = 0;
};

struct HeatmapSamples : public std::array<SampleAverage, (256 - (sizeof(uint32_t) * 2)) / sizeof(SampleAverage)> {
    uint32_t sampleCount, indexOffset;

    static size_t capacity() {
        return std::tuple_size<std::array<SampleAverage, (256 - (sizeof(uint32_t) * 2)) / sizeof(SampleAverage)>>();
    }
};
static_assert(sizeof(HeatmapSamples) == 256, "HeatmapSamples size isn't matched to EventBytes");

struct HeatmapQuery {
    uint64_t rangeStart, rangeSize;
    glm::uvec2 extents;
    glm::ivec3 axisIndex;
};
