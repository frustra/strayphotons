#include "Common.hh"

namespace sp {
    template<typename T, size_t MaxSize>
    struct StackVector {
        StackVector() {}

        void push(const T &value) {
            Assert(offset < MaxSize, "StackVector overflow");
            values[offset++] = value;
        }

        void push(const T *begin, size_t count) {
            Assert(offset + count <= MaxSize, "StackVector overflow");
            std::copy(begin, begin + count, &values[offset]);
            offset += count;
        }

        T *data() {
            return values.data();
        }

        size_t size() const {
            return offset;
        }

    private:
        size_t offset = 0;
        std::array<T, MaxSize> values;
    };
} // namespace sp
