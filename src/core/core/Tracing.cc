#include "Tracing.hh"

#include <cstdlib>

void *operator new(size_t size) {
    auto *ptr = std::malloc(size);
    // This holds a shared_mutex internally that causes multiple threads to block on malloc
    // TracyAlloc(ptr, size);
    return ptr;
}

void operator delete(void *ptr) {
    // TracyFree(ptr);
    std::free(ptr);
}
