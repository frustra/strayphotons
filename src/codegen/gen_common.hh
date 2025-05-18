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

template<typename>
struct CodeGenerator;

template<template<typename...> typename ECSType, typename... AllComponentTypes>
struct CodeGenerator<ECSType<AllComponentTypes...>> {
    static constexpr std::array<std::string_view, sizeof...(AllComponentTypes)> GetComponentNames() {
        return {
            TypeToString<AllComponentTypes>()...,
        };
    }

    static constexpr std::array<bool, sizeof...(AllComponentTypes)> GetComponentGlobalList() {
        return {
            Tecs::is_global_component<AllComponentTypes>()...,
        };
    }

    static constexpr std::array<ecs::StructMetadata, sizeof...(AllComponentTypes)> GetComponentMetadata() {
        return {
            [] {
                ecs::StructMetadata metadata = ecs::StructMetadata::Get<AllComponentTypes>();
                const ecs::StructMetadata *nested = &metadata;
                while (nested) {
                    const ecs::StructMetadata *current = nested;
                    nested = nullptr;
                    for (auto &field : current->fields) {
                        if (field.name.empty()) {
                            Assertf(!nested, "Struct %s has multiple nested metadata types", current->name);
                            nested = ecs::StructMetadata::Get(field.type);
                            if (nested) {
                                metadata.fields.insert(metadata.fields.end(),
                                    nested->fields.begin(),
                                    nested->fields.end());
                                metadata.functions.insert(metadata.functions.end(),
                                    nested->functions.begin(),
                                    nested->functions.end());
                            }
                        }
                    }
                    if (nested == current) break;
                }
                return metadata;
            }()...,
        };
    }
};
