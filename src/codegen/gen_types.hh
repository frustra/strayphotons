#pragma once

#include "ecs/StructFieldTypes.hh"
#include "gen_common.hh"

#include <functional>

std::set<std::type_index> &ReferencedTypes() {
    static std::set<std::type_index> references;
    return references;
}

std::string SnakeCaseTypeName(std::string_view name) {
    std::string snakeCaseName;
    if (sp::starts_with(name, "sp::")) {
        name = name.substr("sp::"s.size());
    }
    if (sp::starts_with(name, "ecs::")) {
        name = name.substr("ecs::"s.size());
        snakeCaseName.append("ecs_");
    }
    bool wasCaps = true;
    for (const char &ch : name) {
        if (ch != std::tolower(ch)) {
            if (!wasCaps) snakeCaseName.append(1, '_');
            wasCaps = true;
        } else {
            wasCaps = false;
        }
        snakeCaseName.append(1, std::tolower(ch));
    }
    return snakeCaseName;
}

std::string StripTypeDecorators(std::string_view name) {
    if (sp::starts_with(name, "sp_")) {
        name = name.substr("sp_"s.size());
    }
    if (sp::ends_with(name, " *")) {
        name = name.substr(0, name.size() - " *"s.size());
    }
    if (sp::ends_with(name, "_t")) {
        name = name.substr(0, name.size() - "_t"s.size());
    }
    return std::string(name);
}

std::string LookupCTypeName(std::type_index type) {
    if (type == typeid(void)) {
        return "void";
    }
    return ecs::GetFieldType(type, [&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;

        if constexpr (std::is_same<T, bool>()) {
            return "bool"s;
        } else if constexpr (std::is_same<T, uint32_t>()) {
            return "uint32_t"s;
        } else if constexpr (std::is_same<T, uint64_t>()) {
            return "uint64_t"s;
        } else if constexpr (std::is_same<T, float>()) {
            return "float"s;
        } else if constexpr (std::is_same<T, double>()) {
            return "double"s;
        } else if constexpr (std::is_same<T, sp::angle_t>()) {
            return "sp_angle_t"s;
        } else if constexpr (std::is_same<T, std::string>()) {
            return "string_t"s;
        } else if constexpr (sp::is_glm_vec<T>()) {
            if constexpr (std::is_same<typename T::value_type, int>()) {
                return "ivec" + std::to_string(T::length()) + "_t";
            } else if constexpr (std::is_same<typename T::value_type, unsigned int>()) {
                return "uvec" + std::to_string(T::length()) + "_t";
            } else if constexpr (std::is_same<typename T::value_type, double>()) {
                return "dvec" + std::to_string(T::length()) + "_t";
            } else {
                return "vec" + std::to_string(T::length()) + "_t";
            }
        } else if constexpr (std::is_same<T, glm::mat3>()) {
            return "mat3_t"s;
        } else if constexpr (std::is_same<T, glm::mat4>()) {
            return "mat4_t"s;
        } else if constexpr (std::is_same<T, glm::quat>()) {
            return "quat_t"s;
        } else if constexpr (std::is_same<T, sp::color_t>()) {
            return "sp_color_t"s;
        } else if constexpr (std::is_same<T, sp::color_alpha_t>()) {
            return "sp_color_alpha_t"s;
        } else if constexpr (sp::is_optional<T>()) {
            return "const " + LookupCTypeName(typeid(T::value_type)) + " *";
        } else if constexpr (sp::is_vector<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::value_type)));
            return "sp_" + subtype + "_vector_t *";
        } else if constexpr (sp::is_pair<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::first_type)));
            if constexpr (!std::is_same<typename T::first_type, typename T::second_type>()) {
                subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(T::second_type)));
            }
            return "sp_" + subtype + "_pair_t *";
        } else if constexpr (sp::is_unordered_map<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::key_type)));
            subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(T::mapped_type)));
            return "sp_" + subtype + "_map_t *";
        } else {
            std::string scn = SnakeCaseTypeName(TypeToString<T>());
            if (ecs::LookupComponent(type)) {
                if (!sp::starts_with(scn, "ecs_")) {
                    scn = "ecs_" + scn;
                }
                return "sp_" + scn + (sp::ends_with(scn, "_t") ? "" : "_t");
            } else if (sp::starts_with(scn, "ecs_")) {
                scn = scn.substr("ecs_"s.size());
            }
            if constexpr (std::is_enum<T>()) {
                return "sp_" + scn + "_t";
            } else if (auto *metadata = ecs::StructMetadata::Get(type); metadata) {
                scn = SnakeCaseTypeName(metadata->name);
                return "sp_" + scn + "_t";
            } else {
                Abortf("Unknown type: %s", scn);
            }
        }
    });
}

std::vector<ecs::StructField> GetTypeFieldList(const ecs::StructMetadata &metadata) {
    std::vector<ecs::StructField> result;
    const ecs::StructMetadata *current = &metadata;
    while (current) {
        const ecs::StructMetadata *nested = nullptr;
        for (auto &field : current->fields) {
            if (field.name.empty()) {
                Assertf(!nested, "Struct %s has multiple nested metadata types", current->name);
                nested = ecs::StructMetadata::Get(field.type);
            } else {
                result.emplace_back(field);
            }
        }
        if (nested == current) break;
        current = nested;
    }
    return result;
}

std::vector<ecs::StructFunction> GetTypeFunctionList(const ecs::StructMetadata &metadata) {
    return metadata.functions;
    // std::vector<ecs::StructFunction> result;
    // const ecs::StructMetadata *current = &metadata;
    // while (current) {
    //     result.insert(result.end(), current->functions.begin(), current->functions.end());
    //     const ecs::StructMetadata *nested = nullptr;
    //     for (auto &field : current->fields) {
    //         if (field.name.empty()) {
    //             Assertf(!nested, "Struct %s has multiple nested metadata types", current->name);
    //             nested = ecs::StructMetadata::Get(field.type);
    //         }
    //     }
    //     if (nested == current) break;
    //     current = nested;
    // }
    // return result;
}

namespace detail {
    template<typename Fn, typename... AllComponentTypes, template<typename...> typename ECSType>
    void forEachComponentType(ECSType<AllComponentTypes...> *, Fn &&callback) {
        (std::invoke(callback, (AllComponentTypes *)nullptr), ...);
    }
} // namespace detail

template<typename Fn>
void ForEachComponentType(Fn &&callback) {
    detail::forEachComponentType((ecs::ECS *)nullptr, std::forward<Fn>(callback));
}

template<typename T>
void GenerateCTypeDefinition(T &out, std::type_index type);

template<typename T>
void GenerateStructWithFields(T &out,
    const std::string &prefixComment,
    const std::string &name,
    const ecs::StructMetadata &metadata) {
    std::vector<const ecs::StructField *> byteMap(metadata.size, nullptr);
    auto fieldList = GetTypeFieldList(metadata);
    for (auto &field : fieldList) {
        for (size_t i = 0; i < field.size; i++) {
            byteMap[field.offset + i] = &field;
        }
    }
    if (!prefixComment.empty()) {
        out << "// " << prefixComment << std::endl;
    }
    out << "typedef struct " << name << " {" << std::endl;
    const ecs::StructField *lastField = nullptr;
    size_t fieldStart = 0;
    for (size_t i = 0; i < byteMap.size(); i++) {
        if (byteMap[i] != lastField) {
            if (lastField) {
                out << "    " << LookupCTypeName(lastField->type) << " " << lastField->name << "; // "
                    << lastField->size << " bytes" << std::endl;
            } else if (i > fieldStart) {
                out << "    uint8_t _unknown" << fieldStart << "[" << (i - fieldStart) << "];" << std::endl;
            }
            lastField = byteMap[i];
            fieldStart = i;
        }
    }
    if (lastField) {
        out << "    " << LookupCTypeName(lastField->type) << " " << lastField->name << "; // " << lastField->size
            << " bytes" << std::endl;
    } else {
        out << "    uint8_t _unknown" << fieldStart << "[" << (metadata.size - fieldStart) << "];" << std::endl;
    }
    out << "} " << name << "; // " << metadata.size << " bytes" << std::endl;
}

template<typename CompType, typename T>
void GenerateCTypeFunctionDefinitions(T &out, const std::string &scn, const std::string &full) {
    auto &metadata = ecs::StructMetadata::Get<CompType>();
    for (auto &func : GetTypeFunctionList(metadata)) {
        std::string functionName = "sp_ecs_"s + scn + "_" + SnakeCaseTypeName(func.name);
        out << "SP_EXPORT " << LookupCTypeName(func.returnType) << " " << functionName << "(";
        out << (func.isConst ? "const " : "") << full << " *comp";
        size_t argI = 0;
        for (auto &arg : func.argTypes) {
            if (argI < func.argDescs.size()) {
                out << ", " << LookupCTypeName(arg) << " " << func.argDescs[argI].name;
            } else {
                out << ", " << LookupCTypeName(arg) << " arg" << argI;
            }
            argI++;
        }
        out << ");" << std::endl;
    }
}

template<typename T>
void GenerateCTypeDefinition(T &out, std::type_index type) {
    if (ReferencedTypes().contains(type)) return;
    ReferencedTypes().emplace(type);
    if (type == typeid(void)) return;
    return ecs::GetFieldType(type, [&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;

        if constexpr (std::is_same<T, bool>()) {
            // Built-in
        } else if constexpr (std::is_same<T, uint32_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, uint64_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, float>()) {
            // Built-in
        } else if constexpr (std::is_same<T, double>()) {
            // Built-in
        } else if constexpr (std::is_same<T, sp::angle_t>()) {
            out << "typedef struct sp_angle_t { float radians; } sp_angle_t;" << std::endl;
        } else if constexpr (std::is_same<T, std::string>()) {
            out << "typedef const char * string_t;" << std::endl;
        } else if constexpr (sp::is_glm_vec<T>()) {
            if constexpr (std::is_same<typename T::value_type, int32_t>()) {
                out << "typedef struct ivec" << std::to_string(T::length()) << "_t { int32_t v["
                    << std::to_string(T::length()) << "]; } ivec" << std::to_string(T::length()) << "_t;" << std::endl;
            } else if constexpr (std::is_same<typename T::value_type, uint32_t>()) {
                out << "typedef struct uvec" << std::to_string(T::length()) << "_t { uint32_t v["
                    << std::to_string(T::length()) << "]; } uvec" << std::to_string(T::length()) << "_t;" << std::endl;
            } else if constexpr (std::is_same<typename T::value_type, double>()) {
                out << "typedef struct dvec" << std::to_string(T::length()) << "_t { double v["
                    << std::to_string(T::length()) << "]; } dvec" << std::to_string(T::length()) << "_t;" << std::endl;
            } else {
                out << "typedef struct vec" << std::to_string(T::length()) << "_t { float v["
                    << std::to_string(T::length()) << "]; } vec" << std::to_string(T::length()) << "_t;" << std::endl;
            }
        } else if constexpr (std::is_same<T, glm::mat3>()) {
            out << "typedef struct mat3_t { float m[3][3]; } mat3_t;" << std::endl;
        } else if constexpr (std::is_same<T, glm::mat4>()) {
            out << "typedef struct mat4_t { float m[4][4]; } mat4_t;" << std::endl;
        } else if constexpr (std::is_same<T, glm::quat>()) {
            out << "typedef struct quat_t { float q[4]; } quat_t;" << std::endl;
        } else if constexpr (std::is_same<T, sp::color_t>()) {
            out << "typedef struct sp_color_t { float rgb[3]; } sp_color_t;" << std::endl;
        } else if constexpr (std::is_same<T, sp::color_alpha_t>()) {
            out << "typedef struct sp_color_alpha_t { float rgba[4]; } sp_color_alpha_t;" << std::endl;
        } else if constexpr (sp::is_optional<T>()) {
            // Skip
        } else if constexpr (sp::is_vector<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::value_type)));
            out << "typedef void *sp_" << subtype << "_vector_t;" << std::endl;
        } else if constexpr (sp::is_pair<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::first_type)));
            if constexpr (!std::is_same<typename T::first_type, typename T::second_type>()) {
                subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(T::second_type)));
            }
            out << "typedef void *sp_" << subtype << "_pair_t;" << std::endl;
        } else if constexpr (sp::is_unordered_map<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::key_type)));
            subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(T::mapped_type)));
            out << "typedef void *sp_" << subtype << "_map_t;" << std::endl;
        } else {
            std::string scn = SnakeCaseTypeName(TypeToString<T>());
            if (const ecs::ComponentBase *comp = ecs::LookupComponent(type); comp) {
                std::string full;
                if (sp::ends_with(scn, "_t")) {
                    scn = scn.substr(0, scn.size() - "_t"s.size());
                }
                if (sp::starts_with(scn, "ecs_")) {
                    full = "sp_" + scn + "_t";
                    scn = scn.substr("ecs_"s.size());
                } else {
                    full = "sp_ecs_" + scn + "_t";
                }

                for (auto &field : GetTypeFieldList(comp->metadata)) {
                    GenerateCTypeDefinition(out, field.type);
                }
                for (auto &func : GetTypeFunctionList(comp->metadata)) {
                    GenerateCTypeDefinition(out, func.returnType);
                    for (auto &arg : func.argTypes) {
                        GenerateCTypeDefinition(out, arg);
                    }
                }

                GenerateStructWithFields(out, "Component: " + comp->metadata.name, full, comp->metadata);
                if (comp->IsGlobal()) {
                    out << "SP_EXPORT " << full << " *sp_ecs_set_" << scn << "(TecsLock *dynLockPtr);" << std::endl;
                    out << "SP_EXPORT " << full << " *sp_ecs_get_" << scn << "(TecsLock *dynLockPtr);" << std::endl;
                    out << "SP_EXPORT const " << full << " *sp_ecs_get_const_" << scn << "(TecsLock *dynLockPtr);"
                        << std::endl;
                    out << "SP_EXPORT void sp_ecs_unset_" << scn << "(TecsLock *dynLockPtr);" << std::endl;
                } else {
                    out << "SP_EXPORT " << full << " *sp_entity_set_" << scn
                        << "(TecsLock *dynLockPtr, sp_entity_t ent);" << std::endl;
                    out << "SP_EXPORT " << full << " *sp_entity_get_" << scn
                        << "(TecsLock *dynLockPtr, sp_entity_t ent);" << std::endl;
                    out << "SP_EXPORT const " << full << " *sp_entity_get_const_" << scn
                        << "(TecsLock *dynLockPtr, sp_entity_t ent);" << std::endl;
                    out << "SP_EXPORT void sp_entity_unset_" << scn << "(TecsLock *dynLockPtr, sp_entity_t ent);"
                        << std::endl;
                }
                GenerateCTypeFunctionDefinitions<T>(out, scn, full);
                out << std::endl;
            } else {
                if (sp::starts_with(scn, "ecs_")) {
                    scn = scn.substr("ecs_"s.size());
                }
                if constexpr (std::is_enum<T>()) {
                    std::string full = "sp_" + scn + "_t";
                    out << "// Enum: " << TypeToString<T>() << std::endl;
                    out << "typedef enum " << full << " {" << std::endl;
                    for (auto &[value, name] : magic_enum::enum_entries<T>()) {
                        std::string valueName = "sp_" + scn + "_" + SnakeCaseTypeName(name);
                        sp::to_upper(valueName);
                        out << "    " << valueName << " = " << static_cast<int>(value) << "," << std::endl;
                    }
                    out << "} " << full << ";" << std::endl;
                    out << std::endl;
                } else if (auto *metadata = ecs::StructMetadata::Get(type); metadata) {
                    for (auto &field : GetTypeFieldList(*metadata)) {
                        GenerateCTypeDefinition(out, field.type);
                    }
                    for (auto &func : GetTypeFunctionList(*metadata)) {
                        GenerateCTypeDefinition(out, func.returnType);
                        for (auto &arg : func.argTypes) {
                            GenerateCTypeDefinition(out, arg);
                        }
                    }

                    scn = SnakeCaseTypeName(metadata->name);
                    std::string full = "sp_" + scn + "_t";
                    GenerateStructWithFields(out, "Type: " + std::string(TypeToString<T>()), full, *metadata);
                    GenerateCTypeFunctionDefinitions<T>(out, scn, full);
                    out << std::endl;
                } else {
                    Abortf("Unknown type definition: %s", TypeToString<T>());
                }
            }
        }
    });
}
