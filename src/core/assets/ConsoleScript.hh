/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "core/Common.hh"

namespace sp {
    class Asset;

    class ConsoleScript : public NonCopyable {
    public:
        ConsoleScript(const string &path, shared_ptr<const Asset> asset);

        const std::vector<string> &Lines() const;

        const string path;

    private:
        shared_ptr<const Asset> asset;
        vector<string> lines;
    };
} // namespace sp
