#include "Tracing.hh"

#include <cstdlib>

void *operator new(size_t size) {
    auto *ptr = std::malloc(size);
    TracyAlloc(ptr, size);
    return ptr;
}

void operator delete(void *ptr) {
    TracyFree(ptr);
    std::free(ptr);
}
