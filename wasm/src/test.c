#include <ecs.h>

extern void hello();

int add(int a, int b) {
    hello();
    return a + b;
}

void onTick(Entity e) {

}

void onSignalChange(Entity e, const char *signal_name, double new_value) {

}
