#pragma once

#include "assets/Gltf.hh"

#include <glm/glm.hpp>
#include <memory>
#include <tiny_gltf.h>
#include <type_traits>

namespace sp::gltf {
    namespace detail {
        template<typename T>
        struct gltfComponentType {};
        template<>
        struct gltfComponentType<int8_t> : std::integral_constant<int, TINYGLTF_COMPONENT_TYPE_BYTE> {};
        template<>
        struct gltfComponentType<uint8_t> : std::integral_constant<int, TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE> {};
        template<>
        struct gltfComponentType<int16_t> : std::integral_constant<int, TINYGLTF_COMPONENT_TYPE_SHORT> {};
        template<>
        struct gltfComponentType<uint16_t> : std::integral_constant<int, TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT> {};
        template<>
        struct gltfComponentType<uint32_t> : std::integral_constant<int, TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT> {};
        template<>
        struct gltfComponentType<float> : std::integral_constant<int, TINYGLTF_COMPONENT_TYPE_FLOAT> {};

        template<typename T, glm::precision P>
        struct gltfComponentType<glm::vec<2, T, P>> : gltfComponentType<T> {};
        template<typename T, glm::precision P>
        struct gltfComponentType<glm::vec<3, T, P>> : gltfComponentType<T> {};
        template<typename T, glm::precision P>
        struct gltfComponentType<glm::vec<4, T, P>> : gltfComponentType<T> {};
        template<typename T, glm::precision P>
        struct gltfComponentType<glm::mat<2, 2, T, P>> : gltfComponentType<T> {};
        template<typename T, glm::precision P>
        struct gltfComponentType<glm::mat<3, 3, T, P>> : gltfComponentType<T> {};
        template<typename T, glm::precision P>
        struct gltfComponentType<glm::mat<4, 4, T, P>> : gltfComponentType<T> {};

        template<typename T>
        struct gltfType : std::integral_constant<int, TINYGLTF_TYPE_SCALAR> {};
        template<typename T, glm::precision P>
        struct gltfType<glm::vec<2, T, P>> : std::integral_constant<int, TINYGLTF_TYPE_VEC2> {};
        template<typename T, glm::precision P>
        struct gltfType<glm::vec<3, T, P>> : std::integral_constant<int, TINYGLTF_TYPE_VEC3> {};
        template<typename T, glm::precision P>
        struct gltfType<glm::vec<4, T, P>> : std::integral_constant<int, TINYGLTF_TYPE_VEC4> {};
        template<typename T, glm::precision P>
        struct gltfType<glm::mat<2, 2, T, P>> : std::integral_constant<int, TINYGLTF_TYPE_MAT2> {};
        template<typename T, glm::precision P>
        struct gltfType<glm::mat<3, 3, T, P>> : std::integral_constant<int, TINYGLTF_TYPE_MAT3> {};
        template<typename T, glm::precision P>
        struct gltfType<glm::mat<4, 4, T, P>> : std::integral_constant<int, TINYGLTF_TYPE_MAT4> {};

        template<int I, typename T, typename... Tn>
        static inline std::pair<int, size_t> GltfTypeInfo(const int &type, const int &componentType) {
            if (type == gltfType<T>() && componentType == gltfComponentType<T>()) return {I, sizeof(T)};
            if constexpr (sizeof...(Tn) == 0) {
                return {-1, 0};
            } else {
                return GltfTypeInfo<I + 1, Tn...>(type, componentType);
            }
        }
    }; // namespace detail

    template<typename ReadT, typename... Tn>
    Accessor<ReadT, Tn...>::Accessor(const Gltf &model, int accessorIndex)
        : Accessor(*model.gltfModel, accessorIndex) {}

    template<typename ReadT, typename... Tn>
    Accessor<ReadT, Tn...>::Accessor(const tinygltf::Model &model, int accessorIndex) {
        if (accessorIndex < 0 || accessorIndex >= model.accessors.size()) {
            Errorf("Gltf::Accessor created with invalid index: %d", accessorIndex);
            return;
        }
        auto &accessor = model.accessors[accessorIndex];
        if (accessor.bufferView < 0 || accessor.bufferView >= model.bufferViews.size()) {
            Errorf("Gltf::Accessor has invalid bufferView index: %d", accessor.bufferView);
            return;
        }
        auto &bufferView = model.bufferViews[accessor.bufferView];
        if (bufferView.buffer < 0 || bufferView.buffer >= model.buffers.size()) {
            Errorf("Gltf::Accessor has invalid buffer index: %d", bufferView.buffer);
            return;
        }

        auto [index, typeSize] = detail::GltfTypeInfo<0, ReadT, Tn...>(accessor.type, accessor.componentType);
        if (index < 0) {
            Errorf("gltf::Accessor is not correct type for %s", typeid(*this).name());
            return;
        }
        typeIndex = index;

        int stride = accessor.ByteStride(bufferView);
        if (stride <= 0) {
            Errorf("gltf::Accessor has invalid byte stride: %d", stride);
            return;
        }
        byteStride = (size_t)stride;
        byteOffset = accessor.byteOffset + bufferView.byteOffset;
        count = accessor.count;

        if (count > 0) {
            auto maxOffset = (count - 1) * byteStride + typeSize;
            if (maxOffset > bufferView.byteLength) {
                Errorf("gltf::Accessor overflows bufferView");
                return;
            } else if (byteOffset + maxOffset > model.buffers[bufferView.buffer].data.size()) {
                Errorf("gltf::Accessor overflows buffer");
                return;
            }
        }

        buffer = &model.buffers[bufferView.buffer];
    }

    template<typename ReadT, typename... Tn>
    size_t Accessor<ReadT, Tn...>::Count() const {
        return *this ? count : 0;
    }

    template<typename ReadT, typename... Tn>
    ReadT Accessor<ReadT, Tn...>::Read(size_t i) const {
        Assertf(buffer && typeIndex >= 0 && typeIndex <= sizeof...(Tn), "Trying to read invalid gltf::Accessor");
        Assertf(i < count, "Trying to read invalid gltf::Accessor index: %u >= %u", i, count);

        std::array<std::function<ReadT()>, 1 + sizeof...(Tn)> convertFuncs = {
            [this, &i] {
                return reinterpret_cast<const ReadT &>(buffer->data[byteOffset + (i * byteStride)]);
            },
            [this, &i] {
                return static_cast<ReadT>(reinterpret_cast<const Tn &>(buffer->data[byteOffset + (i * byteStride)]));
            }...};
        return convertFuncs[typeIndex]();
    }
} // namespace sp::gltf
