/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "gen_components.hh"

int main(int argc, char **argv) {
    if (argc != 4) {
        std::cerr << "Usage: codegen out/components_gen.h out/components_gen_internal.hh out/components_gen.cc"
                  << std::endl;
        return -1;
    }
    {
        auto outH = std::ofstream(argv[1], std::ios::trunc);
        GenerateComponentsH(outH, false);
    }
    {
        auto outH = std::ofstream(argv[2], std::ios::trunc);
        GenerateComponentsH(outH, true);
    }
    {
        auto outCC = std::ofstream(argv[3], std::ios::trunc);
        GenerateComponentsCC(outCC);
    }
}
