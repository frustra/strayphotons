#pragma once

#include "ecs/StructFieldTypes.hh"
#include "ecs/StructMetadata.hh"
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
    if (sp::starts_with(name, "glm::")) {
        name = name.substr("glm::"s.size());
    }
    if (sp::starts_with(name, "std::")) {
        name = name.substr("std::"s.size());
    }
    if (auto sep = name.find("::"); sep != std::string_view::npos) {
        snakeCaseName.append(name.substr(0, sep));
        snakeCaseName.append("_");
        name = name.substr(sep + 2);
    }
    if (auto sep = name.find(" "); sep != std::string_view::npos) {
        snakeCaseName.append(name.substr(0, sep));
        snakeCaseName.append("_");
        name = name.substr(sep + 1);
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

std::string LookupCTypeName(std::type_index type);
std::string LookupCTypeName(uint32_t typeIndex) {
    return LookupCTypeName(ecs::GetFieldTypeIndex(typeIndex));
}

std::string LookupCTypeName(std::type_index type) {
    if (type == typeid(void)) return "void"s;
    return ecs::GetFieldType(type, [&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;

        if constexpr (std::is_same<T, bool>()) {
            return "bool"s;
        } else if constexpr (std::is_same<T, char>()) {
            return "char"s;
        } else if constexpr (std::is_same<T, uint8_t>()) {
            return "uint8_t"s;
        } else if constexpr (std::is_same<T, uint16_t>()) {
            return "uint16_t"s;
        } else if constexpr (std::is_same<T, int32_t>()) {
            return "int32_t"s;
        } else if constexpr (std::is_same<T, uint32_t>()) {
            return "uint32_t"s;
        } else if constexpr (std::is_same<T, uint64_t>()) {
            return "uint64_t"s;
        } else if constexpr (std::is_same<T, size_t>()) {
            return "size_t"s;
        } else if constexpr (std::is_same<T, float>()) {
            return "float"s;
        } else if constexpr (std::is_same<T, double>()) {
            return "double"s;
        } else if constexpr (std::is_same<T, sp::angle_t>()) {
            return "sp_angle_t"s;
        } else if constexpr (std::is_same<T, ecs::EventName>()) {
            return "event_name_t"s;
        } else if constexpr (std::is_same<T, ecs::EventString>()) {
            return "event_string_t"s;
        } else if constexpr (std::is_same<T, ecs::EventBytes>()) {
            return "event_bytes_t"s;
        } else if constexpr (sp::is_inline_string<T>()) {
            return "string_" + std::to_string(T::max_size()) + "_t";
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
        } else if constexpr (std::is_same<T, ecs::EventData>()) {
            return "sp_event_data_t"s;
        } else if constexpr (std::is_same<T, Tecs::Entity>()) {
            return "tecs_entity_t"s;
        } else if constexpr (Tecs::is_lock<T>() || Tecs::is_dynamic_lock<T>()) {
            return "tecs_lock_t"s;
        } else if constexpr (std::is_pointer<T>()) {
            if constexpr (std::is_function<std::remove_pointer_t<T>>()) {
                T funcPtr = nullptr;
                ecs::StructFunction funcInfo = ecs::StructFunction::New("", funcPtr);
                std::string typeName = LookupCTypeName(funcInfo.returnType.typeIndex) + "(*)(";
                for (size_t i = 0; i < funcInfo.argTypes.size(); i++) {
                    if (i > 0) typeName += ", ";
                    typeName += LookupCTypeName(funcInfo.argTypes[i].typeIndex);
                }
                return typeName + ")";
            } else {
                return LookupCTypeName(typeid(std::remove_pointer_t<T>));
            }
        } else if constexpr (sp::is_optional<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(typename T::value_type)));
            return "sp_optional_" + subtype + "_t";
        } else if constexpr (sp::is_vector<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(typename T::value_type)));
            return "sp_" + subtype + "_vector_t";
        } else if constexpr (sp::is_pair<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(typename T::first_type)));
            if constexpr (!std::is_same<typename T::first_type, typename T::second_type>()) {
                subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(typename T::second_type)));
            }
            return "sp_" + subtype + "_pair_t";
        } else if constexpr (sp::is_unordered_flat_map<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(typename T::key_type)));
            subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(typename T::mapped_type)));
            return "sp_" + subtype + "_flatmap_t";
        } else if constexpr (sp::is_unordered_node_map<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(typename T::key_type)));
            subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(typename T::mapped_type)));
            return "sp_" + subtype + "_map_t";
        } else if constexpr (std::is_same<T, sp::GenericCompositor>()) {
            return "sp_compositor_ctx_t"s;
        } else {
            std::string scn = SnakeCaseTypeName(TypeToString<T>());
            Logf("gen_types SnakeCaseTypeName got %s", scn);
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
            result.emplace_back(scn,
                field.desc,
                field.type,
                field.size,
                field.offset,
                field.actions,
                field.functionPointer);
        } else {
            result.emplace_back(field);
        }
    }
    return result;
}

namespace detail {
    template<typename Fn, typename... Tn, template<typename...> typename TemplateType>
    void forEachTemplateType(TemplateType<Tn...> *, Fn &&callback) {
        (std::invoke(callback, (Tn *)nullptr), ...);
    }
} // namespace detail

template<typename Fn>
void ForEachComponentType(Fn &&callback) {
    detail::forEachTemplateType((ecs::ECS *)nullptr, std::forward<Fn>(callback));
}

template<typename Fn>
void ForEachExportedType(Fn &&callback) {
    detail::forEachTemplateType((ecs::FieldTypes *)nullptr, std::forward<Fn>(callback));
}

template<typename Fn>
auto ForEachEventDataType(Fn &&callback) {
    return detail::forEachTemplateType((ecs::EventDataVariant *)nullptr, std::forward<Fn>(callback));
}

template<typename S>
void GenerateCTypeDefinition(S &out, std::type_index type);

std::string ArgTypeToString(const ecs::TypeInfo &info) {
    return (info.isConst ? "const " : "") + LookupCTypeName(info.typeIndex) +
           (info.isPointer || info.isTecsLock ? " *" : "");
}

template<typename S>
void GenerateStructField(S &out, const ecs::StructField &field, const std::string &prefix) {
    if (field.functionPointer) {
        auto &funcInfo = field.functionPointer.value();
        out << prefix << ArgTypeToString(funcInfo.returnType) << "(*" << field.name << ")(";
        for (size_t j = 0; j < funcInfo.argTypes.size(); j++) {
            if (j > 0) out << ", ";
            out << ArgTypeToString(funcInfo.argTypes[j]);
        }
        out << "); // " << field.size << " bytes" << std::endl;
    } else {
        out << prefix << ArgTypeToString(field.type) << " " << field.name << "; // " << field.size << " bytes"
            << std::endl;
    }
}

template<typename S>
void GenerateStructWithFields(S &out,
    const std::string &prefixComment,
    const std::string &name,
    const ecs::StructMetadata &metadata) {
    std::vector<std::set<const ecs::StructField *>> byteMap(metadata.size, std::set<const ecs::StructField *>{});
    auto fieldList = GetTypeFieldList(metadata);
    for (auto &field : fieldList) {
        for (size_t i = 0; i < field.size; i++) {
            byteMap[field.offset + i].emplace(&field);
        }
    }
    if (!prefixComment.empty()) {
        out << "// " << prefixComment << std::endl;
    }
    out << "typedef struct " << name << " {" << std::endl;
    std::set<const ecs::StructField *> lastFields;
    size_t fieldStart = 0;
    for (size_t i = 0; i < byteMap.size(); i++) {
        // Interpret overlapping fields as unions
        bool overlapsLast = lastFields.empty() && byteMap[i].empty();
        for (auto *field : byteMap[i]) {
            if (lastFields.contains(field)) {
                overlapsLast = true;
                break;
            }
        }
        if (overlapsLast && byteMap[i].size() > 1) {
            for (auto *field : byteMap[i]) {
                Assertf(lastFields.contains(field),
                    "Overlapping struct field is incompatible with union: %s",
                    field->name);
            }
        }
        if (!overlapsLast) {
            if (lastFields.size() > 1) {
                out << "    union {" << std::endl;
                for (auto *field : lastFields) {
                    GenerateStructField(out, *field, "        ");
                }
                out << "    }; // " << (i - fieldStart) << " bytes" << std::endl;
            } else if (lastFields.size() == 1) {
                GenerateStructField(out, **lastFields.begin(), "    ");
            } else if (i > fieldStart) {
                out << "    const uint8_t _unknown" << fieldStart << "[" << (i - fieldStart) << "];" << std::endl;
            }
            lastFields = byteMap[i];
            fieldStart = i;
        }
    }
    if (lastFields.size() > 1) {
        out << "    union {" << std::endl;
        for (auto *field : lastFields) {
            GenerateStructField(out, *field, "        ");
        }
        out << "    }; // " << (byteMap.size() - fieldStart) << " bytes" << std::endl;
    } else if (lastFields.size() == 1) {
        GenerateStructField(out, **lastFields.begin(), "    ");
    } else {
        out << "    const uint8_t _unknown" << fieldStart << "[" << (metadata.size - fieldStart) << "];" << std::endl;
    }
    out << "} " << name << "; // " << metadata.size << " bytes" << std::endl;
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
        bool first = true;
        if (!func.isStatic) {
            out << (func.isConst ? "const " : "") << full << " *self";
            first = false;
        }
        size_t argI = 0;
        for (auto &arg : func.argTypes) {
            if (first) {
                first = false;
            } else {
                out << ", ";
            }
            if (argI < func.argDescs.size()) {
                out << ArgTypeToString(arg) << " " << func.argDescs[argI].name;
            } else {
                out << ArgTypeToString(arg) << " arg" << argI;
            }
            argI++;
        }
        if (!func.returnType.isTrivial) {
            if (first) {
                first = false;
            } else {
                out << ", ";
            }
            out << LookupCTypeName(func.returnType.typeIndex) << " *result";
        }
        out << ");" << std::endl;
    }
}

template<typename T, typename S>
void GenerateCppTypeFunctionImplementations(S &out, const std::string &full) {
    if constexpr (std::is_same<T, std::string>()) {
        out << "SP_EXPORT size_t sp_string_get_size(const string_t *str) {" << std::endl;
        out << "    return str->size();" << std::endl;
        out << "}" << std::endl;
        out << std::endl;
        out << "SP_EXPORT const char *sp_string_get_c_str(const string_t *str) {" << std::endl;
        out << "    return str->c_str();" << std::endl;
        out << "}" << std::endl;
        out << std::endl;
        out << "SP_EXPORT char *sp_string_get_data(string_t *str) {" << std::endl;
        out << "    return str->data();" << std::endl;
        out << "}" << std::endl;
        out << std::endl;
        out << "SP_EXPORT void sp_string_set(string_t *str, const char *new_str) {" << std::endl;
        out << "    *str = new_str;" << std::endl;
        out << "}" << std::endl;
        out << std::endl;
        out << "SP_EXPORT int sp_string_compare(const string_t *str, const char *other_str) {" << std::endl;
        out << "    return str->compare(other_str);" << std::endl;
        out << "}" << std::endl;
        out << std::endl;
        out << "SP_EXPORT char *sp_string_resize(string_t *str, size_t new_size, char fill_char) {" << std::endl;
        out << "    str->resize(new_size, fill_char);" << std::endl;
        out << "    return str->data();" << std::endl;
        out << "}" << std::endl;
        out << std::endl;
    } else if constexpr (sp::is_vector<T>()) {
        std::string fullSubtype = LookupCTypeName(typeid(typename T::value_type));
        std::string subtype = StripTypeDecorators(fullSubtype);
        out << "SP_EXPORT size_t sp_" << subtype << "_vector_get_size(const sp_" << subtype << "_vector_t *v) {"
            << std::endl;
        out << "    return v->size();" << std::endl;
        out << "}" << std::endl;
        out << "SP_EXPORT const " << fullSubtype << " *sp_" << subtype << "_vector_get_const_data(const sp_" << subtype
            << "_vector_t *v) {" << std::endl;
        out << "    return v->data();" << std::endl;
        out << "}" << std::endl;
        out << "SP_EXPORT " << fullSubtype << " *sp_" << subtype << "_vector_get_data(sp_" << subtype
            << "_vector_t *v) {" << std::endl;
        out << "    return v->data();" << std::endl;
        out << "}" << std::endl;
        out << "SP_EXPORT " << fullSubtype << " *sp_" << subtype << "_vector_resize(sp_" << subtype
            << "_vector_t *v, size_t new_size) {" << std::endl;
        out << "    v->resize(new_size);" << std::endl;
        out << "    return v->data();" << std::endl;
        out << "}" << std::endl;
        out << std::endl;
    } else {
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
            bool first = true;
            if (!func.isStatic) {
                out << (func.isConst ? "const " : "") << full << " *selfPtr";
                first = false;
            }
            size_t argI = 0;
            for (auto &arg : func.argTypes) {
                if (first) {
                    first = false;
                } else {
                    out << ", ";
                }
                if (argI < func.argDescs.size()) {
                    out << ArgTypeToString(arg) << " " << func.argDescs[argI].name;
                } else {
                    out << ArgTypeToString(arg) << " arg" << argI;
                }
                argI++;
            }
            if (!func.returnType.isTrivial) {
                if (first) {
                    first = false;
                } else {
                    out << ", ";
                }
                out << LookupCTypeName(func.returnType.typeIndex) << " *result";
            }
            out << ") {" << std::endl;
            if (!func.isStatic) {
                out << "    " << (func.isConst ? "const " : "") << TypeToString<T>() << " &self = *reinterpret_cast<"
                    << (func.isConst ? "const " : "") << TypeToString<T>() << " *>(selfPtr);" << std::endl;
            }
            argI = 0;
            for (auto &arg : func.argTypes) {
                if (!arg.isTecsLock) continue;
                out << "    auto tryLock" << argI << " = static_cast<const ecs::DynamicLock<> *>(";
                if (argI < func.argDescs.size()) {
                    out << func.argDescs[argI].name;
                } else {
                    out << "arg" << argI;
                }
                out << ")->TryLock<" << TypeToString(arg.typeIndex) << ">();" << std::endl;
                out << "    Assertf(tryLock" << argI << ", \"" << func.name << " failed to lock "
                    << TypeToString(arg.typeIndex) << "\");" << std::endl;
                argI++;
            }
            if (func.returnType.typeIndex == ecs::GetFieldTypeIndex<void>() && !func.returnType.isPointer) {
                out << "    ";
            } else if (func.returnType.isTrivial) {
                out << "    return static_cast<" << ArgTypeToString(func.returnType) << ">(";
                if (func.returnType.isReference) out << "&";
            } else {
                out << "    *result = static_cast<" << ArgTypeToString(func.returnType) << ">(";
            }
            if (func.isStatic) {
                out << TypeToString<T>() << "::" << func.name << "(";
            } else {
                out << "self." << func.name << "(";
            }
            for (size_t i = 0; i < func.argTypes.size(); i++) {
                if (i > 0) out << ", ";
                if (func.argTypes[i].isTecsLock) {
                    out << "(const " << TypeToString(func.argTypes[i].typeIndex) << " &)*tryLock" << i;
                } else {
                    if (func.argTypes[i].isReference) out << "*";
                    if (i < func.argDescs.size()) {
                        out << func.argDescs[i].name;
                    } else {
                        out << "arg" << i;
                    }
                }
            }
            if (func.returnType.typeIndex != ecs::GetFieldTypeIndex<void>() || func.returnType.isPointer) out << ")";
            out << ");" << std::endl;
            out << "}" << std::endl;
            out << std::endl;
        }
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
void GenerateCTypeDefinition(S &out, std::type_index type);
template<typename S>
void GenerateCTypeDefinition(S &out, uint32_t typeIndex) {
    return GenerateCTypeDefinition(out, ecs::GetFieldTypeIndex(typeIndex));
}

template<typename S>
void GenerateCTypeDefinition(S &out, std::type_index type) {
    if (ReferencedCTypes().contains(type)) return;
    if (type == typeid(void)) {
        out << "const uint32_t SP_TYPE_INDEX_VOID = " << ecs::GetFieldTypeIndex<void>() << ";" << std::endl;
        ReferencedCTypes().emplace(type);
        return;
    }
    return ecs::GetFieldType(type, [&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;

        if constexpr (std::is_same<T, ecs::DynamicLock<>>()) {
            out << "const uint32_t SP_TYPE_INDEX_TECS_LOCK = " << ecs::GetFieldTypeIndex<T>() << ";" << std::endl;
        } else if constexpr (Tecs::is_lock<T>() || Tecs::is_dynamic_lock<T>()) {
            // No-op
        } else if constexpr (std::is_pointer<T>()) {
            if constexpr (std::is_function<std::remove_pointer_t<T>>()) {
                // No-op
            } else {
                auto typeName = StripTypeDecorators(LookupCTypeName(typeid(std::remove_pointer_t<T>)));
                out << "const uint32_t SP_TYPE_INDEX_" << sp::to_upper(typeName)
                    << "_PTR = " << ecs::GetFieldTypeIndex<T>() << ";" << std::endl;
            }
        } else {
            auto typeName = StripTypeDecorators(LookupCTypeName(typeid(T)));
            out << "const uint32_t SP_TYPE_INDEX_" << sp::to_upper(typeName) << " = " << ecs::GetFieldTypeIndex<T>()
                << ";" << std::endl;
        }
        if constexpr (std::is_same<T, bool>()) {
            // Built-in
        } else if constexpr (std::is_same<T, char>()) {
            // Built-in
        } else if constexpr (std::is_same<T, uint8_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, uint16_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, int32_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, uint32_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, uint64_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, size_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, float>()) {
            // Built-in
        } else if constexpr (std::is_same<T, double>()) {
            // Built-in
        } else if constexpr (std::is_same<T, Tecs::Entity>()) {
            // Tecs built-in
        } else if constexpr (Tecs::is_lock<T>() || Tecs::is_dynamic_lock<T>()) {
            // Tecs built-in
        } else if constexpr (std::is_same<T, sp::angle_t>()) {
            out << "typedef struct sp_angle_t { float radians; } sp_angle_t;" << std::endl;
        } else if constexpr (sp::is_inline_string<T>()) {
            out << "typedef char " << LookupCTypeName(typeid(T)) << "[" << (T::max_size() + 1) << "];" << std::endl;
        } else if constexpr (std::is_same<T, ecs::EventBytes>()) {
            out << "typedef uint8_t " << LookupCTypeName(typeid(T)) << "[" << std::tuple_size<T>() << "];" << std::endl;
        } else if constexpr (std::is_same<T, std::string>()) {
            out << "typedef struct string_t { const uint8_t _unknown[" << sizeof(T) << "]; } string_t;" << std::endl;
            out << "SP_EXPORT void sp_string_set(string_t *str, const char *new_str);" << std::endl;
            out << "SP_EXPORT int sp_string_compare(const string_t *str, const char *other_str);" << std::endl;
            out << "SP_EXPORT size_t sp_string_get_size(const string_t *str);" << std::endl;
            out << "SP_EXPORT const char *sp_string_get_c_str(const string_t *str);" << std::endl;
            out << "SP_EXPORT char *sp_string_get_data(string_t *str);" << std::endl;
            out << "SP_EXPORT char *sp_string_resize(string_t *str, size_t new_size, char fill_char);" << std::endl;
            out << std::endl;
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
        } else if constexpr (std::is_pointer<T>()) {
            if constexpr (std::is_function<std::remove_pointer_t<T>>()) {
                T funcPtr = nullptr;
                ecs::StructFunction funcInfo = ecs::StructFunction::New("", funcPtr);
                GenerateCTypeDefinition(out, funcInfo.returnType.typeIndex);
                for (size_t i = 0; i < funcInfo.argTypes.size(); i++) {
                    GenerateCTypeDefinition(out, funcInfo.argTypes[i].typeIndex);
                }
            } else {
                GenerateCTypeDefinition(out, typeid(std::remove_cv_t<std::remove_pointer_t<T>>));
            }
        } else if constexpr (sp::is_optional<T>()) {
            GenerateCTypeDefinition(out, typeid(typename T::value_type));
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(typename T::value_type)));
            out << "typedef struct sp_optional_" << subtype << "_t {" << std::endl;
            out << "    const uint8_t _unknown[" << sizeof(T) << "];" << std::endl;
            out << "} sp_optional_" << subtype << "_t;" << std::endl;
            out << std::endl;
        } else if constexpr (sp::is_vector<T>()) {
            GenerateCTypeDefinition(out, typeid(typename T::value_type));
            std::string fullSubtype = LookupCTypeName(typeid(typename T::value_type));
            std::string subtype = StripTypeDecorators(fullSubtype);
            out << "typedef struct sp_" << subtype << "_vector_t {" << std::endl;
            out << "    const uint8_t _unknown[" << sizeof(T) << "];" << std::endl;
            out << "} sp_" << subtype << "_vector_t;" << std::endl;
            out << "SP_EXPORT size_t sp_" << subtype << "_vector_get_size(const sp_" << subtype << "_vector_t *v);"
                << std::endl;
            out << "SP_EXPORT const " << fullSubtype << " *sp_" << subtype << "_vector_get_const_data(const sp_"
                << subtype << "_vector_t *v);" << std::endl;
            out << "SP_EXPORT " << fullSubtype << " *sp_" << subtype << "_vector_get_data(sp_" << subtype
                << "_vector_t *v);" << std::endl;
            out << "SP_EXPORT " << fullSubtype << " *sp_" << subtype << "_vector_resize(sp_" << subtype
                << "_vector_t *v, size_t new_size);" << std::endl;
            out << std::endl;
        } else if constexpr (sp::is_pair<T>()) {
            GenerateCTypeDefinition(out, typeid(typename T::first_type));
            GenerateCTypeDefinition(out, typeid(typename T::second_type));
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(typename T::first_type)));
            if constexpr (!std::is_same<typename T::first_type, typename T::second_type>()) {
                subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(typename T::second_type)));
            }
            out << "typedef struct sp_" << subtype << "_pair_t {" << std::endl;
            out << "    uint8_t _unknown[" << sizeof(T) << "];" << std::endl;
            out << "} sp_" << subtype << "_pair_t;" << std::endl;
        } else if constexpr (sp::is_unordered_flat_map<T>()) {
            GenerateCTypeDefinition(out, typeid(typename T::key_type));
            GenerateCTypeDefinition(out, typeid(typename T::mapped_type));
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(typename T::key_type)));
            subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(typename T::mapped_type)));
            out << "typedef struct sp_" << subtype << "_flatmap_t {" << std::endl;
            out << "    const uint8_t _unknown[" << sizeof(T) << "];" << std::endl;
            out << "} sp_" << subtype << "_flatmap_t;" << std::endl;
        } else if constexpr (sp::is_unordered_node_map<T>()) {
            GenerateCTypeDefinition(out, typeid(typename T::key_type));
            GenerateCTypeDefinition(out, typeid(typename T::mapped_type));
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(typename T::key_type)));
            subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(typename T::mapped_type)));
            out << "typedef struct sp_" << subtype << "_map_t {" << std::endl;
            out << "    const uint8_t _unknown[" << sizeof(T) << "];" << std::endl;
            out << "} sp_" << subtype << "_map_t;" << std::endl;
        } else if constexpr (std::is_same<T, sp::GenericCompositor>()) {
            // Defined in "strayphotons/graphics.h"
            // out << "typedef void sp_compositor_ctx_t;" << std::endl;
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

                if (ReferencedCTypes().contains(type)) return;
                if constexpr (std::is_enum<T>()) {
                    out << "// Component: " << comp->metadata.name << std::endl;
                    GenerateCEnumDefinition<T>(out, full);
                } else {
                    GenerateStructWithFields(out, "Component: " + comp->metadata.name, full, comp->metadata);
                }
                ReferencedCTypes().emplace(type);
                std::string flagName = sp::to_upper_copy(scn);
                out << "const uint64_t SP_" << flagName << "_INDEX = " << ecs::GetComponentIndex(comp->name) << ";"
                    << std::endl;
                out << "const uint64_t SP_ACCESS_" << flagName << " = 2ull << " << ecs::GetComponentIndex(comp->name)
                    << ";" << std::endl;
                if (comp->IsGlobal()) {
                    out << "SP_EXPORT " << full << " *sp_ecs_set_" << scn << "(tecs_lock_t *dynLockPtr);" << std::endl;
                    out << "SP_EXPORT " << full << " *sp_ecs_get_" << scn << "(tecs_lock_t *dynLockPtr);" << std::endl;
                    out << "SP_EXPORT const " << full << " *sp_ecs_get_const_" << scn << "(tecs_lock_t *dynLockPtr);"
                        << std::endl;
                    out << "SP_EXPORT void sp_ecs_unset_" << scn << "(tecs_lock_t *dynLockPtr);" << std::endl;
                } else {
                    out << "SP_EXPORT " << full << " *sp_entity_set_" << scn
                        << "(tecs_lock_t *dynLockPtr, sp_entity_t ent);" << std::endl;
                    out << "SP_EXPORT " << full << " *sp_entity_get_" << scn
                        << "(tecs_lock_t *dynLockPtr, sp_entity_t ent);" << std::endl;
                    out << "SP_EXPORT const " << full << " *sp_entity_get_const_" << scn
                        << "(tecs_lock_t *dynLockPtr, sp_entity_t ent);" << std::endl;
                    out << "SP_EXPORT void sp_entity_unset_" << scn << "(tecs_lock_t *dynLockPtr, sp_entity_t ent);"
                        << std::endl;
                }

                for (auto &func : comp->metadata.functions) {
                    GenerateCTypeDefinition(out, func.returnType.typeIndex);
                    for (auto &arg : func.argTypes) {
                        GenerateCTypeDefinition(out, arg.typeIndex);
                    }
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
                    if (ReferencedCTypes().contains(type)) return;
                    std::string full = "sp_" + SnakeCaseTypeName(metadata->name) + "_t";
                    GenerateStructWithFields(out, "Type: " + TypeToString<T>(), full, *metadata);
                    ReferencedCTypes().emplace(type);

                    for (auto &func : metadata->functions) {
                        GenerateCTypeDefinition(out, func.returnType.typeIndex);
                        for (auto &arg : func.argTypes) {
                            GenerateCTypeDefinition(out, arg.typeIndex);
                        }
                    }
                    GenerateCTypeFunctionDefinitions<T>(out, full);
                    out << std::endl;
                } else {
                    Abortf("Unknown type definition: %s", TypeToString<T>());
                }
            }
        }
        ReferencedCTypes().emplace(type);
    });
}

template<typename S>
void GenerateCppTypeDefinition(S &out, std::type_index type);
template<typename S>
void GenerateCppTypeDefinition(S &out, uint32_t typeIndex) {
    return GenerateCppTypeDefinition(out, ecs::GetFieldTypeIndex(typeIndex));
}

template<typename S>
void GenerateCppTypeDefinition(S &out, std::type_index type) {
    if (ReferencedCppTypes().contains(type)) return;
    if (type == typeid(void)) return;
    return ecs::GetFieldType(type, [&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;

        if constexpr (std::is_same<T, void *>()) {
            // Built-in
        } else if constexpr (std::is_same<T, bool>()) {
            // Built-in
        } else if constexpr (std::is_same<T, char>()) {
            // Built-in
        } else if constexpr (std::is_same<T, uint8_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, uint16_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, int32_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, uint32_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, uint64_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, size_t>()) {
            // Built-in
        } else if constexpr (std::is_same<T, float>()) {
            // Built-in
        } else if constexpr (std::is_same<T, double>()) {
            // Built-in
        } else if constexpr (std::is_same<T, Tecs::Entity>()) {
            // Tecs built-in
        } else if constexpr (Tecs::is_lock<T>() || Tecs::is_dynamic_lock<T>()) {
            // Tecs built-in
        } else if constexpr (std::is_same<T, sp::angle_t>()) {
            out << "typedef sp::angle_t sp_angle_t;" << std::endl;
        } else if constexpr (std::is_same<T, ecs::EventName>()) {
            out << "typedef ecs::EventName event_name_t;" << std::endl;
        } else if constexpr (std::is_same<T, ecs::EventString>()) {
            out << "typedef ecs::EventString event_string_t;" << std::endl;
        } else if constexpr (std::is_same<T, ecs::EventBytes>()) {
            out << "typedef ecs::EventBytes event_bytes_t;" << std::endl;
        } else if constexpr (sp::is_inline_string<T>()) {
            out << "typedef sp::InlineString<" << T::max_size() << "> string_" << T::max_size() << "_t;" << std::endl;
        } else if constexpr (std::is_same<T, std::string>()) {
            out << "typedef std::string string_t;" << std::endl;
            out << "SP_EXPORT void sp_string_set(string_t *str, const char *new_str);" << std::endl;
            out << "SP_EXPORT int sp_string_compare(const string_t *str, const char *other_str);" << std::endl;
            out << "SP_EXPORT size_t sp_string_get_size(const string_t *str);" << std::endl;
            out << "SP_EXPORT const char *sp_string_get_c_str(const string_t *str);" << std::endl;
            out << "SP_EXPORT char *sp_string_get_data(string_t *str);" << std::endl;
            out << "SP_EXPORT char *sp_string_resize(string_t *str, size_t new_size, char fill_char);" << std::endl;
            out << std::endl;
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
        } else if constexpr (std::is_pointer<T>()) {
            if constexpr (std::is_function<std::remove_pointer_t<T>>()) {
                T funcPtr = nullptr;
                ecs::StructFunction funcInfo = ecs::StructFunction::New("", funcPtr);
                GenerateCppTypeDefinition(out, funcInfo.returnType.typeIndex);
                for (size_t i = 0; i < funcInfo.argTypes.size(); i++) {
                    GenerateCppTypeDefinition(out, funcInfo.argTypes[i].typeIndex);
                }
            } else {
                GenerateCppTypeDefinition(out, typeid(std::remove_cv_t<std::remove_pointer_t<T>>));
            }
        } else if constexpr (sp::is_optional<T>()) {
            GenerateCppTypeDefinition(out, typeid(typename T::value_type));
            std::string subCType = StripTypeDecorators(LookupCTypeName(typeid(typename T::value_type)));
            std::string subCppType = TypeToString<typename T::value_type>();
            out << "typedef std::optional<" << subCppType << "> sp_optional_" << subCType << "_t;" << std::endl;
        } else if constexpr (sp::is_vector<T>()) {
            GenerateCppTypeDefinition(out, typeid(typename T::value_type));
            std::string fullCSubtype = LookupCTypeName(typeid(typename T::value_type));
            std::string subCType = StripTypeDecorators(fullCSubtype);
            std::string subCppType = TypeToString<typename T::value_type>();
            out << "typedef std::vector<" << subCppType << "> sp_" << subCType << "_vector_t;" << std::endl;
            out << "SP_EXPORT size_t sp_" << subCType << "_vector_get_size(const sp_" << subCType << "_vector_t *v);"
                << std::endl;
            out << "SP_EXPORT const " << fullCSubtype << " *sp_" << subCType << "_vector_get_const_data(const sp_"
                << subCType << "_vector_t *v);" << std::endl;
            out << "SP_EXPORT " << fullCSubtype << " *sp_" << subCType << "_vector_get_data(sp_" << subCType
                << "_vector_t *v);" << std::endl;
            out << "SP_EXPORT " << fullCSubtype << " *sp_" << subCType << "_vector_resize(sp_" << subCType
                << "_vector_t *v, size_t new_size);" << std::endl;
            out << std::endl;
        } else if constexpr (sp::is_pair<T>()) {
            GenerateCppTypeDefinition(out, typeid(typename T::first_type));
            GenerateCppTypeDefinition(out, typeid(typename T::second_type));
            std::string subCType = StripTypeDecorators(LookupCTypeName(typeid(typename T::first_type)));
            if constexpr (!std::is_same<typename T::first_type, typename T::second_type>()) {
                subCType += "_" + StripTypeDecorators(LookupCTypeName(typeid(typename T::second_type)));
            }
            std::string subCppType = TypeToString<typename T::first_type>() + ", " +
                                     TypeToString<typename T::second_type>();
            out << "typedef std::pair<" << subCppType << "> sp_" << subCType << "_pair_t;" << std::endl;
        } else if constexpr (sp::is_unordered_flat_map<T>()) {
            GenerateCppTypeDefinition(out, typeid(typename T::key_type));
            GenerateCppTypeDefinition(out, typeid(typename T::mapped_type));
            std::string subCType = StripTypeDecorators(LookupCTypeName(typeid(typename T::key_type)));
            subCType += "_" + StripTypeDecorators(LookupCTypeName(typeid(typename T::mapped_type)));
            std::string subCppType = TypeToString<typename T::key_type>() + ", " +
                                     TypeToString<typename T::mapped_type>();
            out << "typedef robin_hood::unordered_flat_map<" << subCppType << "> sp_" << subCType << "_flatmap_t;"
                << std::endl;
        } else if constexpr (sp::is_unordered_node_map<T>()) {
            GenerateCppTypeDefinition(out, typeid(typename T::key_type));
            GenerateCppTypeDefinition(out, typeid(typename T::mapped_type));
            std::string subCType = StripTypeDecorators(LookupCTypeName(typeid(typename T::key_type)));
            subCType += "_" + StripTypeDecorators(LookupCTypeName(typeid(typename T::mapped_type)));
            std::string subCppType = TypeToString<typename T::key_type>() + ", " +
                                     TypeToString<typename T::mapped_type>();
            out << "typedef robin_hood::unordered_node_map<" << subCppType << "> sp_" << subCType << "_map_t;"
                << std::endl;
        } else if constexpr (std::is_same<T, sp::GenericCompositor>()) {
            // Defined in "strayphotons/graphics.h"
            // out << "typedef sp::GenericCompositor sp_compositor_ctx_t;" << std::endl;
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

                if (!ReferencedCppTypes().contains(type)) {
                    out << "// Component: " << comp->metadata.name << std::endl;
                    out << "typedef " << TypeToString<T>() << " " << full << ";" << std::endl;
                    ReferencedCppTypes().emplace(type);
                }
                std::string flagName = sp::to_upper_copy(scn);
                out << "const uint64_t SP_" << flagName << "_INDEX = " << ecs::GetComponentIndex(comp->name) << ";"
                    << std::endl;
                out << "const uint64_t SP_ACCESS_" << flagName << " = 2ull <<" << ecs::GetComponentIndex(comp->name)
                    << ";" << std::endl;
                if (comp->IsGlobal()) {
                    out << "SP_EXPORT " << full << " *sp_ecs_set_" << scn << "(tecs_lock_t *dynLockPtr);" << std::endl;
                    out << "SP_EXPORT " << full << " *sp_ecs_get_" << scn << "(tecs_lock_t *dynLockPtr);" << std::endl;
                    out << "SP_EXPORT const " << full << " *sp_ecs_get_const_" << scn << "(tecs_lock_t *dynLockPtr);"
                        << std::endl;
                    out << "SP_EXPORT void sp_ecs_unset_" << scn << "(tecs_lock_t *dynLockPtr);" << std::endl;
                } else {
                    out << "SP_EXPORT " << full << " *sp_entity_set_" << scn
                        << "(tecs_lock_t *dynLockPtr, sp_entity_t ent);" << std::endl;
                    out << "SP_EXPORT " << full << " *sp_entity_get_" << scn
                        << "(tecs_lock_t *dynLockPtr, sp_entity_t ent);" << std::endl;
                    out << "SP_EXPORT const " << full << " *sp_entity_get_const_" << scn
                        << "(tecs_lock_t *dynLockPtr, sp_entity_t ent);" << std::endl;
                    out << "SP_EXPORT void sp_entity_unset_" << scn << "(tecs_lock_t *dynLockPtr, sp_entity_t ent);"
                        << std::endl;
                }

                for (auto &func : comp->metadata.functions) {
                    GenerateCppTypeDefinition(out, func.returnType.typeIndex);
                    for (auto &arg : func.argTypes) {
                        GenerateCppTypeDefinition(out, arg.typeIndex);
                    }
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
                    std::string full = "sp_" + SnakeCaseTypeName(metadata->name) + "_t";
                    if (ReferencedCppTypes().contains(type)) return;
                    out << "// Type: " << TypeToString<T>() << std::endl;
                    out << "typedef " << TypeToString<T>() << " " << full << ";" << std::endl;
                    ReferencedCppTypes().emplace(type);

                    for (auto &func : metadata->functions) {
                        GenerateCppTypeDefinition(out, func.returnType.typeIndex);
                        for (auto &arg : func.argTypes) {
                            GenerateCppTypeDefinition(out, arg.typeIndex);
                        }
                    }
                    GenerateCTypeFunctionDefinitions<T>(out, full);
                    out << std::endl;
                } else {
                    Abortf("Unknown type definition: %s", TypeToString<T>());
                }
            }
        }
        ReferencedCppTypes().emplace(type);
    });
}
