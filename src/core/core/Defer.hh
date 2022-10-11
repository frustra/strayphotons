#pragma once

namespace sp {
    template<typename Fn>
    struct Defer {
        Defer(Fn &&fn) : fn(std::move(fn)) {}
        ~Defer() {
            fn();
        }
        Fn fn;
    };
} // namespace sp
