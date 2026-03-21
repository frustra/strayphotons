/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "gen_components.hh"

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: strayphotons-exports-components-codegen out/components.h out/components.cc" << std::endl;
        return -1;
    }
    {
        auto outH = std::ofstream(argv[1], std::ios::trunc);
        GenerateComponentsH(outH);
    }
    {
        auto outCC = std::ofstream(argv[2], std::ios::trunc);
        GenerateComponentsCC(outCC);
    }
}
