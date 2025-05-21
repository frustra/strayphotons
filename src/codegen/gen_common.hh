#pragma once

#include <Tecs.hh>
#include <ecs/EcsImpl.hh>
#include <fstream>
#include <iostream>
#include <memory>
#include <source_location>

template<typename T>
auto EmbedTypeIntoSignature() {
    return std::string_view{std::source_location::current().function_name()};
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
    } else {
        auto dummyInt = EmbedTypeIntoSignature<int>();
        auto intStart = dummyInt.find("int");
        auto tailLength = dummyInt.size() - intStart - "int"s.size();

        auto typeStart = intStart;
        auto embeddingSignature = EmbedTypeIntoSignature<T>();
        auto enumStart = embeddingSignature.find("enum ", intStart);
        if (enumStart == intStart) typeStart += "enum "s.size();
        auto classStart = embeddingSignature.find("class ", intStart);
        if (classStart == intStart) typeStart += "class "s.size();
        auto structStart = embeddingSignature.find("struct ", intStart);
        if (structStart == intStart) typeStart += "struct "s.size();

        auto typeLength = embeddingSignature.size() - typeStart - tailLength;
        return std::string(embeddingSignature.substr(typeStart, typeLength));
    }
}
