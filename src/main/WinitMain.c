#include <stdio.h>
#include <strayphotons.h>

int main(int argc, char **argv) {
    printf("sp-winit starting...\n");
    StrayPhotons instance = game_init(argc, argv);
    if (!instance) return 1;
    int result = game_start(instance);
    game_destroy(instance);
    return result;
}
