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
auto TypeToString() {
    auto dummyInt = EmbedTypeIntoSignature<int>();
    auto intStart = dummyInt.find("int");
    auto tailLength = dummyInt.size() - intStart - std::string("int").length();

    auto typeStart = intStart;
    auto embeddingSignature = EmbedTypeIntoSignature<T>();
    auto enumStart = embeddingSignature.find("enum ", intStart);
    if (enumStart == intStart) typeStart += std::string("enum ").length();
    auto classStart = embeddingSignature.find("class ", intStart);
    if (classStart == intStart) typeStart += std::string("class ").length();
    auto structStart = embeddingSignature.find("struct ", intStart);
    if (structStart == intStart) typeStart += std::string("struct ").length();

    auto typeLength = embeddingSignature.size() - typeStart - tailLength;
    return embeddingSignature.substr(typeStart, typeLength);
}
