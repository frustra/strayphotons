/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <glm/glm.hpp>
#include <iostream>

namespace sp {
    template<typename T>
    std::istream &operator>>(std::istream &is, glm::tvec2<T, glm::highp> &v) {
        return is >> v[0] >> v[1];
    }

    template<typename T>
    std::istream &operator>>(std::istream &is, glm::tvec3<T, glm::highp> &v) {
        return is >> v[0] >> v[1] >> v[2];
    }

    template<typename T>
    std::ostream &operator<<(std::ostream &os, glm::tvec2<T, glm::highp> &v) {
        return os << v[0] << " " << v[1];
    }

    template<typename T>
    std::ostream &operator<<(std::ostream &os, glm::tvec3<T, glm::highp> &v) {
        return os << v[0] << " " << v[1] << " " << v[2];
    }
} // namespace sp
