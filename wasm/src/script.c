#include "../../src/core/ecs/components/Transform.h"

extern void hello(Transform vec);

Transform add(Transform a, Transform b) {
    hello(a);
    hello(b);
    GlmVec3 pos;
    transform_get_position(&b, &pos);
    Transform x;
    transform_from_pos(&pos, &x);
    return x;
}
