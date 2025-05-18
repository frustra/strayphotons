#include "gen_components.hh"

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: codegen out/components.h out/components.cc";
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
