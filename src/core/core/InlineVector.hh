#pragma once

#include "Common.hh"

#include <array>

namespace sp {
    template<typename T, size_t MaxSize, typename ArrayT = std::array<T, MaxSize>>
    class InlineVector : private ArrayT {
    public:
        using value_type = ArrayT::value_type;
        using size_type = ArrayT::size_type;
        using difference_type = ArrayT::difference_type;
        using reference = ArrayT::reference;
        using const_reference = ArrayT::const_reference;
        using pointer = ArrayT::pointer;
        using const_pointer = ArrayT::const_pointer;
        using iterator = ArrayT::iterator;
        using const_iterator = ArrayT::const_iterator;
        using reverse_iterator = ArrayT::reverse_iterator;
        using const_reverse_iterator = ArrayT::const_reverse_iterator;
        using ArrayT::at;
        using ArrayT::data;
        using ArrayT::front;
        using ArrayT::max_size;
        using ArrayT::operator[];

        size_type size() const {
            return offset;
        }

        bool empty() const {
            return !offset;
        }

        T &back() {
            return at(offset - 1);
        }

        void push_back(const T &value) {
            Assert(offset < MaxSize, "InlineVector overflow");
            at(offset++) = value;
        }

        void pop_back() {
            Assert(offset > 0, "InlineVector underflow");
            offset -= 1;
        }

        template<class... Args>
        T &emplace_back(Args &&...args) {
            Assert(offset + 1 <= MaxSize, "InlineVector overflow");
            auto &ref = at(offset++);
            ref = T(std::forward<Args>(args)...);
            return ref;
        }

        iterator insert(const_iterator pos, const T &value) {
            return insert(pos, &value, &value + 1);
        }
        template<class InputIt>
        iterator insert(const_iterator pos, InputIt first, InputIt last) {
            auto n = std::distance(first, last);
            Assert(offset + n <= MaxSize, "InlineVector overflow");

            auto posOffset = pos - begin();
            T *posPtr = &at(posOffset);
            T *oldEndPtr = &at(offset);

            if (posPtr < oldEndPtr) std::memmove(posPtr + n, posPtr, (oldEndPtr - posPtr) * sizeof(T));
            std::copy(first, last, posPtr);
            offset += n;
            return begin() + posOffset;
        }
        iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
            return insert(pos, ilist.begin(), ilist.end());
        }

        using ArrayT::begin;
        iterator end() {
            return begin() + offset;
        }
        const_iterator end() const {
            return begin() + offset;
        }

        using ArrayT::rend;
        reverse_iterator rbegin() {
            return rend() - offset;
        }
        const_reverse_iterator rbegin() const {
            return rend() - offset;
        }

    private:
        size_type offset = 0;
    };
} // namespace sp
