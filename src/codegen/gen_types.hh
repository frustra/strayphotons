#pragma once

#include "ecs/StructFieldTypes.hh"
#include "gen_common.hh"

#include <functional>

std::set<std::type_index> &ReferencedCTypes() {
    static std::set<std::type_index> references;
    return references;
}

std::set<std::type_index> &ReferencedCppTypes() {
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
            return "const " + LookupCTypeName(typeid(typename T::value_type)) + " *";
        } else if constexpr (sp::is_vector<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(typename T::value_type)));
            return "sp_" + subtype + "_vector_t *";
        } else if constexpr (sp::is_pair<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(typename T::first_type)));
            if constexpr (!std::is_same<typename T::first_type, typename T::second_type>()) {
                subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(typename T::second_type)));
            }
            return "sp_" + subtype + "_pair_t *";
        } else if constexpr (sp::is_unordered_flat_map<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(typename T::key_type)));
            subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(typename T::mapped_type)));
            return "sp_" + subtype + "_flatmap_t *";
        } else if constexpr (sp::is_unordered_node_map<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(typename T::key_type)));
            subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(typename T::mapped_type)));
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
    for (auto &field : metadata.fields) {
        if (field.name.empty()) {
            if (field.type == metadata.type) continue;
            std::string scn = LookupCTypeName(field.type);
            if (sp::starts_with(scn, "sp_")) scn = scn.substr("sp_"s.size());
            if (sp::starts_with(scn, "ecs_")) scn = scn.substr("ecs_"s.size());
            if (sp::ends_with(scn, " *")) scn = scn.substr(0, scn.size() - " *"s.size());
            if (sp::ends_with(scn, "_t")) scn = scn.substr(0, scn.size() - "_t"s.size());
            auto i = scn.find("_vector");
            if (i != std::string::npos) {
                scn = scn.substr(0, i) + "s" + scn.substr(i + "_vector"s.size());
            }
            result.emplace_back(scn, field.desc, field.type, field.size, field.offset, field.actions);
        } else {
            result.emplace_back(field);
        }
    }
    return result;
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

template<typename S>
void GenerateCTypeDefinition(S &out, std::type_index type);

template<typename S>
void GenerateStructWithFields(S &out,
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

std::string ArgTypeToString(const ecs::TypeInfo &info) {
    return (info.isConst ? "const " : "") + LookupCTypeName(info.type) + (info.isPointer ? " *" : "");
}

template<typename T, typename S>
void GenerateCTypeFunctionDefinitions(S &out, const std::string &full) {
    auto &metadata = ecs::StructMetadata::Get<T>();
    std::string scn = (sp::starts_with(full, "sp_") ? full.substr("sp_"s.size()) : full);
    if (sp::ends_with(scn, "_t")) scn = scn.substr(0, scn.size() - "_t"s.size());
    for (auto &func : metadata.functions) {
        std::string functionName = "sp_"s + scn + "_" + SnakeCaseTypeName(func.name);
        out << "SP_EXPORT ";
        if (func.returnType.isTrivial) {
            out << ArgTypeToString(func.returnType);
        } else {
            out << "void";
        }
        out << " " << functionName << "(";
        out << (func.isConst ? "const " : "") << full << " *self";
        size_t argI = 0;
        for (auto &arg : func.argTypes) {
            if (argI < func.argDescs.size()) {
                out << ", " << ArgTypeToString(arg) << " " << func.argDescs[argI].name;
            } else {
                out << ", " << ArgTypeToString(arg) << " arg" << argI;
            }
            argI++;
        }
        if (!func.returnType.isTrivial) {
            out << ", " << LookupCTypeName(func.returnType.type) << " *result";
        }
        out << ");" << std::endl;
    }
}

template<typename T, typename S>
void GenerateCppTypeFunctionImplementations(S &out, const std::string &full) {
    auto *metadata = ecs::StructMetadata::Get(typeid(T));
    if (!metadata) return;
    std::string scn = (sp::starts_with(full, "sp_") ? full.substr("sp_"s.size()) : full);
    if (sp::ends_with(scn, "_t")) scn = scn.substr(0, scn.size() - "_t"s.size());
    for (auto &func : metadata->functions) {
        std::string functionName = "sp_"s + scn + "_" + SnakeCaseTypeName(func.name);
        out << "SP_EXPORT ";
        if (func.returnType.isTrivial) {
            out << ArgTypeToString(func.returnType);
        } else {
            out << "void";
        }
        out << " " << functionName << "(";
        out << (func.isConst ? "const " : "") << full << " *selfPtr";
        size_t argI = 0;
        for (auto &arg : func.argTypes) {
            if (argI < func.argDescs.size()) {
                out << ", " << ArgTypeToString(arg) << " " << func.argDescs[argI].name;
            } else {
                out << ", " << ArgTypeToString(arg) << " arg" << argI;
            }
            argI++;
        }
        if (!func.returnType.isTrivial) {
            out << ", " << LookupCTypeName(func.returnType.type) << " *result";
        }
        out << ") {" << std::endl;
        if (!func.isStatic) {
            out << "    " << (func.isConst ? "const " : "") << TypeToString<T>() << " &self = *reinterpret_cast<"
                << (func.isConst ? "const " : "") << TypeToString<T>() << " *>(selfPtr);" << std::endl;
        }
        if (func.returnType.type == typeid(void)) {
            out << "    ";
        } else if (func.returnType.isTrivial) {
            out << "    return ";
            if (func.returnType.isReference) out << "&";
        } else {
            out << "    *result = ";
        }
        if (func.isStatic) {
            out << TypeToString<T>() << "::" << func.name << "(";
        } else {
            out << "self." << func.name << "(";
        }
        for (size_t i = 0; i < func.argTypes.size(); i++) {
            if (i > 0) out << ", ";
            std::string argTypeName = ecs::GetFieldType(func.argTypes[i].type, [&](auto *typePtr) {
                using T = std::remove_pointer_t<decltype(typePtr)>;
                return TypeToString<T>();
            });
            // out << "reinterpret_cast<" << argTypeName << " &>(";
            if (!func.argTypes[i].isTrivial || func.argTypes[i].isReference) out << "*";
            if (i < func.argDescs.size()) {
                out << func.argDescs[i].name;
            } else {
                out << "arg" << i;
            }
            // out << ")";
        }
        out << ");" << std::endl;
        out << "}" << std::endl;
        out << std::endl;
    }
}

template<typename T, typename S>
void GenerateCEnumDefinition(S &out, const std::string &full) {
    std::string scn = (sp::starts_with(full, "sp_") ? full.substr("sp_"s.size()) : full);
    if (sp::ends_with(scn, "_t")) scn = scn.substr(0, scn.size() - "_t"s.size());
    if constexpr (sizeof(T) == sizeof(int)) {
        out << "typedef enum " << full << " {" << std::endl;
        for (auto &[value, name] : magic_enum::enum_entries<T>()) {
            std::string valueName = "sp_" + scn + "_" + SnakeCaseTypeName(name);
            sp::to_upper(valueName);
            out << "    " << valueName << " = " << static_cast<int>(value) << "," << std::endl;
        }
        out << "} " << full << ";" << std::endl;
    } else {
        for (auto &[value, name] : magic_enum::enum_entries<T>()) {
            std::string valueName = "sp_" + scn + "_" + SnakeCaseTypeName(name);
            sp::to_upper(valueName);
            out << "const " << TypeToString<magic_enum::underlying_type_t<T>>() << " " << valueName << " = "
                << static_cast<
                       std::conditional_t<std::is_signed_v<magic_enum::underlying_type_t<T>>, int64_t, uint64_t>>(value)
                << ";" << std::endl;
        }
        out << "typedef " << TypeToString<magic_enum::underlying_type_t<T>>() << " " << full << ";" << std::endl;
    }
}

template<typename S>
void GenerateCTypeDefinition(S &out, std::type_index type) {
    if (ReferencedCTypes().contains(type)) return;
    ReferencedCTypes().emplace(type);
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
            out << "typedef struct string_t { uint8_t _unknown[" << sizeof(T) << "]; } string_t;" << std::endl;
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
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::value_type)));
            out << "typedef struct sp_optional_" << subtype << "_t {" << std::endl;
            out << "    uint8_t _unknown[" << sizeof(T) << "];" << std::endl;
            out << "} sp_optional_" << subtype << "_t;" << std::endl;
        } else if constexpr (sp::is_vector<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::value_type)));
            out << "typedef struct sp_" << subtype << "_vector_t {" << std::endl;
            out << "    uint8_t _unknown[" << sizeof(T) << "];" << std::endl;
            out << "} sp_" << subtype << "_vector_t;" << std::endl;
        } else if constexpr (sp::is_pair<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::first_type)));
            if constexpr (!std::is_same<typename T::first_type, typename T::second_type>()) {
                subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(T::second_type)));
            }
            out << "typedef struct sp_" << subtype << "_pair_t {" << std::endl;
            out << "    uint8_t _unknown[" << sizeof(T) << "];" << std::endl;
            out << "} sp_" << subtype << "_pair_t;" << std::endl;
        } else if constexpr (sp::is_unordered_flat_map<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::key_type)));
            subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(T::mapped_type)));
            out << "typedef struct sp_" << subtype << "_flatmap_t {" << std::endl;
            out << "    uint8_t _unknown[" << sizeof(T) << "];" << std::endl;
            out << "} sp_" << subtype << "_flatmap_t;" << std::endl;
        } else if constexpr (sp::is_unordered_node_map<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::key_type)));
            subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(T::mapped_type)));
            out << "typedef struct sp_" << subtype << "_map_t {" << std::endl;
            out << "    uint8_t _unknown[" << sizeof(T) << "];" << std::endl;
            out << "} sp_" << subtype << "_map_t;" << std::endl;
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
                for (auto &func : comp->metadata.functions) {
                    GenerateCTypeDefinition(out, func.returnType.type);
                    for (auto &arg : func.argTypes) {
                        GenerateCTypeDefinition(out, arg.type);
                    }
                }

                if constexpr (std::is_enum<T>()) {
                    out << "// Component: " << comp->metadata.name << std::endl;
                    GenerateCEnumDefinition<T>(out, full);
                } else {
                    GenerateStructWithFields(out, "Component: " + comp->metadata.name, full, comp->metadata);
                }
                std::string flagName = sp::to_upper_copy(scn);
                out << "const uint64_t SP_" << flagName << "_INDEX = " << ecs::GetComponentIndex(comp->name) << ";"
                    << std::endl;
                out << "const uint64_t SP_ACCESS_" << flagName << " = 2ull << SP_" << flagName << "_INDEX;"
                    << std::endl;
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
                GenerateCTypeFunctionDefinitions<T>(out, full);
                out << std::endl;
            } else {
                if (sp::starts_with(scn, "ecs_")) {
                    scn = scn.substr("ecs_"s.size());
                }
                if constexpr (std::is_enum<T>()) {
                    std::string full = "sp_" + scn + "_t";
                    out << "// Enum: " << TypeToString<T>() << std::endl;
                    GenerateCEnumDefinition<T>(out, full);
                } else if (auto *metadata = ecs::StructMetadata::Get(type); metadata) {
                    for (auto &field : GetTypeFieldList(*metadata)) {
                        GenerateCTypeDefinition(out, field.type);
                    }
                    for (auto &func : metadata->functions) {
                        GenerateCTypeDefinition(out, func.returnType.type);
                        for (auto &arg : func.argTypes) {
                            GenerateCTypeDefinition(out, arg.type);
                        }
                    }

                    std::string full = "sp_" + SnakeCaseTypeName(metadata->name) + "_t";
                    GenerateStructWithFields(out, "Type: " + TypeToString<T>(), full, *metadata);
                    GenerateCTypeFunctionDefinitions<T>(out, full);
                    out << std::endl;
                } else {
                    Abortf("Unknown type definition: %s", TypeToString<T>());
                }
            }
        }
    });
}

template<typename S>
void GenerateCppTypeDefinition(S &out, std::type_index type) {
    if (ReferencedCppTypes().contains(type)) return;
    ReferencedCppTypes().emplace(type);
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
            out << "typedef sp::angle_t sp_angle_t;" << std::endl;
        } else if constexpr (std::is_same<T, std::string>()) {
            out << "typedef std::string string_t;" << std::endl;
        } else if constexpr (sp::is_glm_vec<T>()) {
            if constexpr (std::is_same<typename T::value_type, int32_t>()) {
                out << "typedef glm::ivec" << std::to_string(T::length()) << " ivec" << std::to_string(T::length())
                    << "_t;" << std::endl;
            } else if constexpr (std::is_same<typename T::value_type, uint32_t>()) {
                out << "typedef glm::uvec" << std::to_string(T::length()) << " uvec" << std::to_string(T::length())
                    << "_t;" << std::endl;
            } else if constexpr (std::is_same<typename T::value_type, double>()) {
                out << "typedef glm::dvec" << std::to_string(T::length()) << " dvec" << std::to_string(T::length())
                    << "_t;" << std::endl;
            } else {
                out << "typedef glm::vec" << std::to_string(T::length()) << " vec" << std::to_string(T::length())
                    << "_t;" << std::endl;
            }
        } else if constexpr (std::is_same<T, glm::mat3>()) {
            out << "typedef glm::mat3 mat3_t;" << std::endl;
        } else if constexpr (std::is_same<T, glm::mat4>()) {
            out << "typedef glm::mat4 mat4_t;" << std::endl;
        } else if constexpr (std::is_same<T, glm::quat>()) {
            out << "typedef glm::quat quat_t;" << std::endl;
        } else if constexpr (std::is_same<T, sp::color_t>()) {
            out << "typedef sp::color_t sp_color_t;" << std::endl;
        } else if constexpr (std::is_same<T, sp::color_alpha_t>()) {
            out << "typedef sp::color_alpha_t sp_color_alpha_t;" << std::endl;
        } else if constexpr (sp::is_optional<T>()) {
            std::string subCType = StripTypeDecorators(LookupCTypeName(typeid(T::value_type)));
            std::string subCppType = TypeToString<typename T::value_type>();
            out << "typedef std::optional<" << subCppType << "> sp_optional_" << subCType << "_t;" << std::endl;
        } else if constexpr (sp::is_vector<T>()) {
            std::string subCType = StripTypeDecorators(LookupCTypeName(typeid(T::value_type)));
            std::string subCppType = TypeToString<typename T::value_type>();
            out << "typedef std::vector<" << subCppType << "> sp_" << subCType << "_vector_t;" << std::endl;
        } else if constexpr (sp::is_pair<T>()) {
            std::string subCType = StripTypeDecorators(LookupCTypeName(typeid(T::first_type)));
            if constexpr (!std::is_same<typename T::first_type, typename T::second_type>()) {
                subCType += "_" + StripTypeDecorators(LookupCTypeName(typeid(T::second_type)));
            }
            std::string subCppType = TypeToString<typename T::first_type>() + ", " +
                                     TypeToString<typename T::second_type>();
            out << "typedef std::pair<" << subCppType << "> sp_" << subCType << "_pair_t;" << std::endl;
        } else if constexpr (sp::is_unordered_flat_map<T>()) {
            std::string subCType = StripTypeDecorators(LookupCTypeName(typeid(T::key_type)));
            subCType += "_" + StripTypeDecorators(LookupCTypeName(typeid(T::mapped_type)));
            std::string subCppType = TypeToString<typename T::key_type>() + ", " +
                                     TypeToString<typename T::mapped_type>();
            out << "typedef robin_hood::unordered_flat_map<" << subCppType << "> sp_" << subCType << "_flatmap_t;"
                << std::endl;
        } else if constexpr (sp::is_unordered_node_map<T>()) {
            std::string subCType = StripTypeDecorators(LookupCTypeName(typeid(T::key_type)));
            subCType += "_" + StripTypeDecorators(LookupCTypeName(typeid(T::mapped_type)));
            std::string subCppType = TypeToString<typename T::key_type>() + ", " +
                                     TypeToString<typename T::mapped_type>();
            out << "typedef robin_hood::unordered_node_map<" << subCppType << "> sp_" << subCType << "_map_t;"
                << std::endl;
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
                    GenerateCppTypeDefinition(out, field.type);
                }
                for (auto &func : comp->metadata.functions) {
                    GenerateCppTypeDefinition(out, func.returnType.type);
                    for (auto &arg : func.argTypes) {
                        GenerateCppTypeDefinition(out, arg.type);
                    }
                }

                out << "// Component: " << comp->metadata.name << std::endl;
                out << "typedef " << TypeToString<T>() << " " << full << ";" << std::endl;
                std::string flagName = sp::to_upper_copy(scn);
                out << "const uint64_t SP_" << flagName << "_INDEX = " << ecs::GetComponentIndex(comp->name) << ";"
                    << std::endl;
                out << "const uint64_t SP_ACCESS_" << flagName << " = 2ull << SP_" << flagName << "_INDEX;"
                    << std::endl;
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
                GenerateCTypeFunctionDefinitions<T>(out, full);
                out << std::endl;
            } else {
                if (sp::starts_with(scn, "ecs_")) {
                    scn = scn.substr("ecs_"s.size());
                }
                if constexpr (std::is_enum<T>()) {
                    std::string full = "sp_" + scn + "_t";
                    out << "// Enum: " << TypeToString<T>() << std::endl;
                    out << "typedef " << TypeToString<T>() << " " << full << ";" << std::endl;
                } else if (auto *metadata = ecs::StructMetadata::Get(type); metadata) {
                    for (auto &field : GetTypeFieldList(*metadata)) {
                        GenerateCppTypeDefinition(out, field.type);
                    }
                    for (auto &func : metadata->functions) {
                        GenerateCppTypeDefinition(out, func.returnType.type);
                        for (auto &arg : func.argTypes) {
                            GenerateCppTypeDefinition(out, arg.type);
                        }
                    }

                    std::string full = "sp_" + SnakeCaseTypeName(metadata->name) + "_t";
                    out << "// Type: " << TypeToString<T>() << std::endl;
                    out << "typedef " << TypeToString<T>() << " " << full << ";" << std::endl;
                    GenerateCTypeFunctionDefinitions<T>(out, full);
                    out << std::endl;
                } else {
                    Abortf("Unknown type definition: %s", TypeToString<T>());
                }
            }
        }
    });
}
