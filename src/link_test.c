#include <stdio.h>
#include <strayphotons.h>

int main(int argc, char **argv) {
    printf("C Linker test starting\n");
    StrayPhotons instance = game_init(argc, argv);
    int result = game_start(instance);
    game_destroy(instance);
    return result;
}
