#pragma once

#include "ecs/StructFieldTypes.hh"
#include "gen_common.hh"

std::set<std::type_index> &TypeReferences() {
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

std::string LookupCTypeName(std::type_index idx) {
    if (idx == typeid(void)) {
        return "void";
    }
    TypeReferences().emplace(idx);
    return ecs::GetFieldType(idx, [&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;

        if constexpr (std::is_same<T, uint32_t>()) {
            return "uint32_t"s;
        } else if constexpr (std::is_same<T, uint64_t>()) {
            return "uint64_t"s;
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
            if (ecs::LookupComponent(idx)) {
                if (!sp::starts_with(scn, "ecs_")) {
                    scn = "ecs_" + scn;
                }
                return "sp_" + scn + (sp::ends_with(scn, "_t") ? "" : "_t");
            } else if (sp::starts_with(scn, "ecs_")) {
                scn = scn.substr("ecs_"s.size());
            }
            if constexpr (std::is_enum<T>()) {
                return (sp::starts_with(scn, "sp_") ? "" : "sp_") + scn + (sp::ends_with(scn, "_t") ? "" : "_t");
            } else if (auto *metadata = ecs::StructMetadata::Get(idx); metadata) {
                scn = SnakeCaseTypeName(metadata->name);
                return (sp::starts_with(scn, "sp_") ? "" : "sp_") + scn + (sp::ends_with(scn, "_t") ? "" : "_t");
            } else {
                return SnakeCaseTypeName(TypeToString<T>());
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
    std::vector<ecs::StructFunction> result;
    const ecs::StructMetadata *current = &metadata;
    while (current) {
        result.insert(result.end(), current->functions.begin(), current->functions.end());
        const ecs::StructMetadata *nested = nullptr;
        for (auto &field : current->fields) {
            if (field.name.empty()) {
                Assertf(!nested, "Struct %s has multiple nested metadata types", current->name);
                nested = ecs::StructMetadata::Get(field.type);
            }
        }
        if (nested == current) break;
        current = nested;
    }
    return result;
}

namespace detail {
    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    void forEachComponent(ECSType<AllComponentTypes...> *,
        std::function<void(const std::string &, const ecs::ComponentBase &)> &callback) {
        ( // For each component:
            [&] {
                using T = AllComponentTypes;
                auto base = ecs::LookupComponent(std::type_index(typeid(T)));
                Assertf(base != nullptr, "ForEachComponent Couldn't lookup component type: %s", typeid(T).name());
                callback(base->name, *base);
            }(),
            ...);
    }
} // namespace detail

void ForEachComponent(std::function<void(const std::string &, const ecs::ComponentBase &)> callback) {
    detail::forEachComponent((ecs::ECS *)nullptr, callback);
}

std::string LookupCTypeDefinition(std::type_index idx) {
    if (idx == typeid(void)) {
        return "";
    }
    return ecs::GetFieldType(idx, [&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;

        std::stringstream out;
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
        } else if constexpr (std::is_same<T, std::string>()) {
            out << "typedef const char *string_t;";
        } else if constexpr (sp::is_glm_vec<T>()) {
            if constexpr (std::is_same<typename T::value_type, int32_t>()) {
                out << "typedef struct ivec" << std::to_string(T::length()) << "_t { int32_t v["
                    << std::to_string(T::length()) << "]; } ivec" << std::to_string(T::length()) << "_t;";
            } else if constexpr (std::is_same<typename T::value_type, uint32_t>()) {
                out << "typedef struct uvec" << std::to_string(T::length()) << "_t { uint32_t v["
                    << std::to_string(T::length()) << "]; } uvec" << std::to_string(T::length()) << "_t;";
            } else if constexpr (std::is_same<typename T::value_type, double>()) {
                out << "typedef struct dvec" << std::to_string(T::length()) << "_t { double v["
                    << std::to_string(T::length()) << "]; } dvec" << std::to_string(T::length()) << "_t;";
            } else {
                out << "typedef struct vec" << std::to_string(T::length()) << "_t { float v["
                    << std::to_string(T::length()) << "]; } vec" << std::to_string(T::length()) << "_t;";
            }
        } else if constexpr (std::is_same<T, glm::mat3>()) {
            out << "typedef struct mat3_t { float m[3][3]; } mat3_t;";
        } else if constexpr (std::is_same<T, glm::mat4>()) {
            out << "typedef struct mat4_t { float m[4][4]; } mat4_t;";
        } else if constexpr (std::is_same<T, glm::quat>()) {
            out << "typedef struct quat_t { float q[4]; } quat_t;";
        } else if constexpr (sp::is_optional<T>()) {
            // Skip
        } else if constexpr (sp::is_vector<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::value_type)));
            out << "typedef void *sp_" << subtype << "_vector_t;";
        } else if constexpr (sp::is_pair<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::first_type)));
            if constexpr (!std::is_same<typename T::first_type, typename T::second_type>()) {
                subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(T::second_type)));
            }
            out << "typedef void *sp_" << subtype << "_pair_t;";
        } else if constexpr (sp::is_unordered_map<T>()) {
            std::string subtype = StripTypeDecorators(LookupCTypeName(typeid(T::key_type)));
            subtype += "_" + StripTypeDecorators(LookupCTypeName(typeid(T::mapped_type)));
            out << "typedef void *sp_" << subtype << "_map_t;";
        } else {
            std::string scn = SnakeCaseTypeName(TypeToString<T>());
            if (const ecs::ComponentBase *comp = ecs::LookupComponent(idx); comp) {
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

                out << std::endl;
                out << "// Component: " << comp->name << std::endl;
                out << "typedef struct " << full << " {" << std::endl;
                out << "    uint8_t data[" << comp->metadata.size << "];" << std::endl;
                out << "} " << full << ";" << std::endl;
                out << std::endl;
                if (comp->IsGlobal()) {
                    out << "SP_EXPORT " << full << " *sp_ecs_set_" << scn << "(TecsLock *dynLockPtr);" << std::endl;
                    out << "SP_EXPORT " << full << " *sp_ecs_get_" << scn << "(TecsLock *dynLockPtr);" << std::endl;
                    out << "SP_EXPORT const " << full << " *sp_ecs_get_const_" << scn << "(TecsLock *dynLockPtr);"
                        << std::endl;
                    out << "SP_EXPORT void sp_ecs_unset_" << scn << "(TecsLock *dynLockPtr);";
                } else {
                    out << "SP_EXPORT " << full << " *sp_entity_set_" << scn
                        << "(TecsLock *dynLockPtr, sp_entity_t ent);" << std::endl;
                    out << "SP_EXPORT " << full << " *sp_entity_get_" << scn
                        << "(TecsLock *dynLockPtr, sp_entity_t ent);" << std::endl;
                    out << "SP_EXPORT const " << full << " *sp_entity_get_const_" << scn
                        << "(TecsLock *dynLockPtr, sp_entity_t ent);" << std::endl;
                    out << "SP_EXPORT void sp_entity_unset_" << scn << "(TecsLock *dynLockPtr, sp_entity_t ent);";
                }
                for (auto &field : GetTypeFieldList(comp->metadata)) {
                    out << std::endl;
                    out << "// Field: " << LookupCTypeName(field.type) << " " << field.name << ";";
                }
                for (auto &func : GetTypeFunctionList(comp->metadata)) {
                    std::string functionName = std::string(comp->IsGlobal() ? "sp_ecs_" : "sp_entity_") + scn + "_" +
                                               SnakeCaseTypeName(func.name);
                    out << std::endl;
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
                    out << ");";
                }
            } else {
                if (sp::starts_with(scn, "ecs_")) {
                    scn = scn.substr("ecs_"s.size());
                }
                if constexpr (std::is_enum<T>()) {
                    out << "// Enum: "
                        << (sp::starts_with(scn, "sp_") ? "" : "sp_") + scn + (sp::ends_with(scn, "_t") ? "" : "_t");
                } else if (auto *metadata = ecs::StructMetadata::Get(idx); metadata) {
                    scn = SnakeCaseTypeName(metadata->name);
                    std::string full = (sp::starts_with(scn, "sp_") ? "" : "sp_") + scn +
                                       (sp::ends_with(scn, "_t") ? "" : "_t");
                    out << "typedef struct " << full << " { // " << TypeToString<T>() << std::endl;
                    out << "    uint8_t data[" << sizeof(T) << "];" << std::endl;
                    out << "} " << full << ";";
                } else {
                    out << "// Other: " << SnakeCaseTypeName(TypeToString<T>());
                }
            }
        }
        return out.str();
    });
}
