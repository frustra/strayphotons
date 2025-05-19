#pragma once

#include "gen_common.hh"
#include "gen_types.hh"

#include <ranges>

template<typename T>
void GenerateComponentsH(T &out) {
    out << R"RAWSTR(#pragma once

#include <c_abi/Tecs.h>
#include <strayphotons/entity.h>
#include <strayphotons/export.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)
)RAWSTR";
    ForEachComponentType([&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;
        auto &comp = ecs::LookupComponent<T>();
        GenerateCTypeDefinition(out, comp.metadata.type);
    });
    out << R"RAWSTR(
#pragma pack(pop)
#ifdef __cplusplus
} // extern "C"
#endif
)RAWSTR";
}

template<typename T>
void GenerateComponentsCC(T &out) {
    out << R"RAWSTR(#include <strayphotons/components.h>
#include <ecs/EcsImpl.hh>

using DynamicLock = Tecs::DynamicLock<ecs::ECS>;

extern "C" {

)RAWSTR";
    ForEachComponentType([&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;
        std::string name(TypeToString<T>());
        std::string scn = SnakeCaseTypeName(name);
        std::string full;
        if (sp::starts_with(scn, "ecs_")) {
            full = "sp_" + scn + "_t";
            scn = scn.substr("ecs_"s.size());
        } else {
            full = "sp_ecs_" + scn + "_t";
        }
        out << "/**" << std::endl;
        out << " * Component: " << name << std::endl;
        out << " */" << std::endl;
        // clang-format off
        if constexpr (Tecs::is_global_component<T>()) {
    out << "SP_EXPORT " << full << " *sp_ecs_set_" << scn << "(TecsLock *dynLockPtr) {"                                                     << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                             << std::endl;
    out << "    Assertf(dynLock, \"sp_ecs_set_" << scn << "() called with null lock\");"                                                    << std::endl;
    out << "    auto lock1 = dynLock->TryLock<Tecs::AddRemove>();"                                                                          << std::endl;
    out << "    if (lock1) {"                                                                                                               << std::endl;
    out << "        return reinterpret_cast<" << full << " *>(&lock1->Set<" << name << ">());"                                              << std::endl;
    out << "    }"                                                                                                                          << std::endl;
    out << "    auto lock2 = dynLock->TryLock<Tecs::Write<" << name << ">>();"                                                              << std::endl;
    out << "    Assertf(lock2, \"Lock does not have " << name << " write permissions\");"                                                   << std::endl;
    out << "    return reinterpret_cast<" << full << " *>(&lock2->Set<" << name << ">());"                                                  << std::endl;
    out << "}"                                                                                                                              << std::endl;
    out                                                                                                                                     << std::endl;
    out << "SP_EXPORT " << full << " *sp_ecs_get_" << scn << "(TecsLock *dynLockPtr) {"                                                     << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                             << std::endl;
    out << "    Assertf(dynLock, \"sp_ecs_get_" << scn << "() called with null lock\");"                                                    << std::endl;
    out << "    auto lock1 = dynLock->TryLock<Tecs::AddRemove>();"                                                                          << std::endl;
    out << "    if (lock1) {"                                                                                                               << std::endl;
    out << "        return reinterpret_cast<" << full << " *>(&lock1->Get<" << name << ">());"                                              << std::endl;
    out << "    }"                                                                                                                          << std::endl;
    out << "    auto lock2 = dynLock->TryLock<Tecs::Write<" << name << ">>();"                                                              << std::endl;
    out << "    Assertf(lock2, \"Lock does not have " << name << " write permissions\");"                                                   << std::endl;
    out << "    return reinterpret_cast<" << full << " *>(&lock2->Get<" << name << ">());"                                                  << std::endl;
    out << "}"                                                                                                                              << std::endl;
    out                                                                                                                                     << std::endl;
    out << "SP_EXPORT const " << full << " *sp_ecs_get_const_" << scn << "(TecsLock *dynLockPtr) {"                                         << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                             << std::endl;
    out << "    Assertf(dynLock, \"sp_ecs_get_const_" << scn << "() called with null lock\");"                                              << std::endl;
    out << "    auto lock = dynLock->TryLock<Tecs::Read<" << name << ">>();"                                                                << std::endl;
    out << "    Assertf(lock, \"Lock does not have " << name << " read permissions\");"                                                     << std::endl;
    out << "    return reinterpret_cast<const " << full << " *>(&lock->Get<const " << name << ">());"                                       << std::endl;
    out << "}"                                                                                                                              << std::endl;
    out                                                                                                                                     << std::endl;
    out << "SP_EXPORT void sp_ecs_unset_" << scn << "(TecsLock *dynLockPtr) {"                                                              << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                             << std::endl;
    out << "    Assertf(dynLock, \"sp_ecs_unset_" << scn << "() called with null lock\");"                                                  << std::endl;
    out << "    auto lock = dynLock->TryLock<Tecs::AddRemove>();"                                                                           << std::endl;
    out << "    Assertf(lock, \"Lock does not have AddRemove permissions\");"                                                               << std::endl;
    out << "    lock->Unset<" << name << ">();"                                                                                             << std::endl;
    out << "}"                                                                                                                              << std::endl;
    out                                                                                                                                     << std::endl;
        } else {
    out << "SP_EXPORT " << full << " *sp_entity_set_" << scn << "(TecsLock *dynLockPtr, sp_entity_t ent) {"                                 << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                             << std::endl;
    out << "    Assertf(dynLock, \"sp_entity_set_" << scn << "() called with null lock\");"                                                 << std::endl;
    out << "    auto lock1 = dynLock->TryLock<Tecs::AddRemove>();"                                                                          << std::endl;
    out << "    if (lock1) {"                                                                                                               << std::endl;
    out << "        return reinterpret_cast<" << full << " *>(&Tecs::Entity(ent).Set<" << name << ">(*lock1));"                             << std::endl;
    out << "    }"                                                                                                                          << std::endl;
    out << "    auto lock2 = dynLock->TryLock<Tecs::Write<" << name << ">>();"                                                              << std::endl;
    out << "    Assertf(lock2, \"Lock does not have " << name << " write permissions\");"                                                   << std::endl;
    out << "    return reinterpret_cast<" << full << " *>(&Tecs::Entity(ent).Set<" << name << ">(*lock2));"                                 << std::endl;
    out << "}"                                                                                                                              << std::endl;
    out                                                                                                                                     << std::endl;
    out << "SP_EXPORT " << full << " *sp_entity_get_" << scn << "(TecsLock *dynLockPtr, sp_entity_t ent) {"                                 << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                             << std::endl;
    out << "    Assertf(dynLock, \"sp_entity_get_" << scn << "() called with null lock\");"                                                 << std::endl;
    out << "    auto lock1 = dynLock->TryLock<Tecs::AddRemove>();"                                                                          << std::endl;
    out << "    if (lock1) {"                                                                                                               << std::endl;
    out << "        return reinterpret_cast<" << full << " *>(&Tecs::Entity(ent).Get<" << name << ">(*lock1));"                             << std::endl;
    out << "    }"                                                                                                                          << std::endl;
    out << "    auto lock2 = dynLock->TryLock<Tecs::Write<" << name << ">>();"                                                              << std::endl;
    out << "    Assertf(lock2, \"Lock does not have " << name << " write permissions\");"                                                   << std::endl;
    out << "    return reinterpret_cast<" << full << " *>(&Tecs::Entity(ent).Get<" << name << ">(*lock2));"                                 << std::endl;
    out << "}"                                                                                                                              << std::endl;
    out                                                                                                                                     << std::endl;
    out << "SP_EXPORT const " << full << " *sp_entity_get_const_" << scn << "(TecsLock *dynLockPtr, sp_entity_t ent) {"                     << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                             << std::endl;
    out << "    Assertf(dynLock, \"sp_entity_get_const_" << scn << "() called with null lock\");"                                           << std::endl;
    out << "    auto lock = dynLock->TryLock<Tecs::Read<" << name << ">>();"                                                                << std::endl;
    out << "    Assertf(lock, \"Lock does not have " << name << " read permissions\");"                                                     << std::endl;
    out << "    return reinterpret_cast<const " << full << " *>(&Tecs::Entity(ent).Get<const " << name << ">(*lock));"                      << std::endl;
    out << "}"                                                                                                                              << std::endl;
    out                                                                                                                                     << std::endl;
    out << "SP_EXPORT void sp_entity_unset_" << scn << "(TecsLock *dynLockPtr, sp_entity_t ent) {"                                          << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                             << std::endl;
    out << "    Assertf(dynLock, \"sp_entity_unset_" << scn << "() called with null lock\");"                                               << std::endl;
    out << "    auto lock = dynLock->TryLock<Tecs::AddRemove>();"                                                                           << std::endl;
    out << "    Assertf(lock, \"Lock does not have AddRemove permissions\");"                                                               << std::endl;
    out << "    Tecs::Entity(ent).Unset<" << name << ">(*lock);"                                                                            << std::endl;
    out << "}"                                                                                                                              << std::endl;
    out                                                                                                                                     << std::endl;
        }
        // clang-format on
        auto &metadata = ecs::StructMetadata::Get<T>();
        auto functionList = GetTypeFunctionList(metadata);
        for (auto &func : functionList) {
            std::string functionName = "sp_ecs_"s + scn + "_" + SnakeCaseTypeName(func.name);
            std::string returnTypeName(LookupCTypeName(func.returnType));
            out << "SP_EXPORT " << returnTypeName << " " << functionName << "(";
            out << (func.isConst ? "const " : "") << full << " *compPtr";
            size_t argI = 0;
            for (auto &arg : func.argTypes) {
                if (argI < func.argDescs.size()) {
                    out << ", " << LookupCTypeName(arg) << " " << func.argDescs[argI].name;
                } else {
                    out << ", " << LookupCTypeName(arg) << " arg" << argI;
                }
                argI++;
            }
            out << ") {" << std::endl;
            if (!func.isStatic) {
                out << "    " << (func.isConst ? "const " : "") << TypeToString<T>()
                    << " &component = *reinterpret_cast<" << (func.isConst ? "const " : "") << TypeToString<T>()
                    << " *>(compPtr);" << std::endl;
            }
            if (func.returnType != typeid(void)) {
                out << "    return reinterpret_cast<" << returnTypeName << ">(";
            } else {
                out << "    ";
            }
            if (func.isStatic) {
                out << TypeToString<T>() << "::" << func.name << "(";
            } else {
                out << "component." << func.name << "(";
            }
            for (size_t i = 0; i < func.argTypes.size(); i++) {
                if (i > 0) out << ", ";
                if (i < func.argDescs.size()) {
                    out << func.argDescs[i].name;
                } else {
                    out << "arg" << i;
                }
            }
            if (func.returnType != typeid(void)) {
                out << "));" << std::endl;
            } else {
                out << ");" << std::endl;
            }
            out << "}" << std::endl;
            out << std::endl;
        }
    });

    out << "} // extern \"C\"" << std::endl;
}
