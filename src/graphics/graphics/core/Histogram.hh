/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"

namespace sp {
    template<size_t BucketCount>
    struct Histogram {
        std::array<uint32, BucketCount> buckets;
        uint64 min = 0, max = 0, count = 0;

        void Reset(uint64 newMin, uint64 newMax) {
            min = newMin;
            max = std::max(min + 1, newMax);
            count = 0;
            for (auto &v : buckets)
                v = 0;
        }

        void AddSample(uint64 sample) {
            // linear distribution of buckets between min and max
            size_t index = (sample - min) * (buckets.size() - 1) / (max - min);
            buckets[std::min(std::max(index, size_t(0)), buckets.size() - 1)]++;
            count++;
        }

        uint64 GetPercentile(uint64 percentile) {
            uint64 target = percentile * count / 100;
            uint64 sum = 0;
            for (size_t i = 0; i < buckets.size(); i++) {
                sum += buckets[i];
                if (sum >= target) return (i * 2 + 1) * (max - min) / 2 / (buckets.size() - 1) + min;
            }
            return 0;
        }
    };
} // namespace sp
