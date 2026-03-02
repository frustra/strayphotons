/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "strayphotons/cpp/Utility.hh"

namespace sp {
    class Asset;

    class ConsoleScript : public NonCopyable {
    public:
        ConsoleScript(const std::string &path, std::shared_ptr<const Asset> asset);

        const std::vector<std::string> &Lines() const;

        const std::string path;

    private:
        std::shared_ptr<const Asset> asset;
        std::vector<std::string> lines;
    };
} // namespace sp
