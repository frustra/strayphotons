#include <ecs.h>
#include <ecs/components/Transform.h>

// If this changes, make sure it is the same in C++ and Rust
static_assert(sizeof(ecs::Transform) == 48, "Wrong Transform size");

int add(int a, int b) {
    return a + b;
}

void onTick(Entity e) {}

void onSignalChange(Entity e, const char *signal_name, double new_value) {}
