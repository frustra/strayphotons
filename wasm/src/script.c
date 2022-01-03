#include <ecs/components/Transform.h>

extern void hello(Transform vec);

Transform add(Transform a, Transform b) {
    hello(a);
    hello(b);
    GlmVec3 pos = {5, 6, 7};
    transform_set_position(&a, &pos);
    transform_get_position(&pos, &b);
    Transform x;
    transform_from_pos(&x, &pos);
    return x;
}
