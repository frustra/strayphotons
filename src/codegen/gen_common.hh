#pragma once

#include <Tecs.hh>
#include <cstring>
#include <ecs/EcsImpl.hh>
#include <ecs/StructFieldTypes.hh>
#include <fstream>
#include <iostream>
#include <memory>
#include <source_location>

template<typename T>
auto EmbedTypeIntoSignature() {
    const char *funcName = std::source_location::current().function_name();
    return std::string_view(std::strcmp("EmbedTypeIntoSignature", funcName) == 0 ? typeid(T).name() : funcName);
}

template<typename T>
std::string TypeToString() {
    if constexpr (sp::is_glm_vec<T>()) {
        if constexpr (std::is_same<typename T::value_type, int32_t>()) {
            return "glm::ivec" + std::to_string(T::length());
        } else if constexpr (std::is_same<typename T::value_type, uint32_t>()) {
            return "glm::uvec" + std::to_string(T::length());
        } else if constexpr (std::is_same<typename T::value_type, double>()) {
            return "glm::dvec" + std::to_string(T::length());
        } else {
            return "glm::vec" + std::to_string(T::length());
        }
    } else if constexpr (std::is_same<T, glm::mat3>()) {
        return "glm::mat3";
    } else if constexpr (std::is_same<T, glm::mat4>()) {
        return "glm::mat4";
    } else if constexpr (std::is_same<T, glm::quat>()) {
        return "glm::quat";
    } else if constexpr (std::is_same<T, ecs::EventData>()) {
        return "ecs::EventData";
    } else if constexpr (sp::is_inline_string<T>()) {
        if constexpr (std::is_same<T, ecs::EventName>()) {
            return "ecs::EventName";
        } else if constexpr (std::is_same<T, ecs::EventString>()) {
            return "ecs::EventString";
        } else if constexpr (std::is_same<T, ecs::EventBytes>()) {
            return "ecs::EventBytes";
        } else if constexpr (std::is_same<typename T::value_type, char>()) {
            return "sp::InlineString<" + std::to_string(T::max_size()) + ">";
        } else {
            Abortf("Unsupported InlineString type: %s", typeid(T::value_type).name());
        }
    } else if constexpr (std::is_same<T, std::string>()) {
        return "std::string";
    } else if constexpr (std::is_same<T, ecs::Lock<>>()) {
        return "ecs::Lock<>";
    } else if constexpr (Tecs::is_lock<T>()) {
        auto dummyChar = EmbedTypeIntoSignature<ecs::Lock<unsigned char>>();
        auto charStart = dummyChar.find("unsigned char");
        auto tailLength = dummyChar.size() - charStart - "unsigned char"s.size();

        auto typeStart = charStart;
        auto embeddingSignature = EmbedTypeIntoSignature<T>();
        auto structStart = embeddingSignature.find("struct ", charStart);
        if (structStart == charStart) typeStart += "struct "s.size();

        auto typeLength = embeddingSignature.size() - typeStart - tailLength;
        return "ecs::Lock<" + std::string(embeddingSignature.substr(typeStart, typeLength)) + ">";
    } else if constexpr (std::is_same<T, ecs::DynamicLock<>>()) {
        return "ecs::DynamicLock<>";
    } else if constexpr (Tecs::is_dynamic_lock<T>()) {
        auto dummyChar = EmbedTypeIntoSignature<ecs::DynamicLock<unsigned char>>();
        auto charStart = dummyChar.find("unsigned char");
        auto tailLength = dummyChar.size() - charStart - "unsigned char"s.size();

        auto typeStart = charStart;
        auto embeddingSignature = EmbedTypeIntoSignature<T>();
        auto structStart = embeddingSignature.find("struct ", charStart);
        if (structStart == charStart) typeStart += "struct "s.size();

        auto typeLength = embeddingSignature.size() - typeStart - tailLength;
        return "ecs::DynamicLock<" + std::string(embeddingSignature.substr(typeStart, typeLength)) + ">";
    } else {
        auto dummyChar = EmbedTypeIntoSignature<unsigned char>();
        auto charStart = dummyChar.find("unsigned char");
        auto tailLength = dummyChar.size() - charStart - "unsigned char"s.size();

        auto typeStart = charStart;
        auto embeddingSignature = EmbedTypeIntoSignature<T>();
        auto enumStart = embeddingSignature.find("enum ", charStart);
        if (enumStart == charStart) typeStart += "enum "s.size();
        auto classStart = embeddingSignature.find("class ", charStart);
        if (classStart == charStart) typeStart += "class "s.size();
        auto structStart = embeddingSignature.find("struct ", charStart);
        if (structStart == charStart) typeStart += "struct "s.size();

        auto typeLength = embeddingSignature.size() - typeStart - tailLength;
        return std::string(embeddingSignature.substr(typeStart, typeLength));
    }
}

std::string TypeToString(std::type_index type) {
    if (type == typeid(void)) return "void";
    return ecs::GetFieldType(type, [&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;
        return TypeToString<T>();
    });
}

std::string TypeToString(uint32_t typeIndex) {
    return ecs::GetFieldType(typeIndex, [&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;
        return TypeToString<T>();
    });
}
