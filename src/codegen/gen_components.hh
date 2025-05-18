#pragma once

#include "gen_common.hh"
#include "gen_types.hh"

template<typename T>
void GenerateComponentsH(T &finalOutput) {
    std::stringstream componentStream;
    {
        auto &out = componentStream;
        ForEachComponent([&](const std::string &name, const ecs::ComponentBase &comp) {
            auto def = LookupCTypeDefinition(comp.metadata.type);
            if (!def.empty()) out << def << std::endl;
        });
    }
    std::stringstream typeStream;
    {
        auto &out = typeStream;
        for (auto &idx : TypeReferences()) {
            auto def = LookupCTypeDefinition(idx);
            if (!def.empty()) out << def << std::endl;
        }
    }

    finalOutput << R"RAWSTR(#pragma once

#include <c_abi/Tecs.h>
#include <strayphotons/entity.h>
#include <strayphotons/export.h>

#ifdef __cplusplus
extern "C" {
#endif

)RAWSTR";
    finalOutput << typeStream.str();
    finalOutput << componentStream.str();
    finalOutput << R"RAWSTR(#ifdef __cplusplus
} // extern "C"
#endif
)RAWSTR";
}

template<typename T>
void GenerateComponentsCC(T &out) {
    auto names = CodeGenerator<ecs::ECS>::GetComponentNames();
    auto globalList = CodeGenerator<ecs::ECS>::GetComponentGlobalList();
    out << R"RAWSTR(#include <strayphotons/components.h>
#include <ecs/EcsImpl.hh>

)RAWSTR";
    out << "using ECS = " << TypeToString<ecs::ECS>();
    out << R"RAWSTR(;
using DynamicLock = Tecs::DynamicLock<ECS>;

extern "C" {

)RAWSTR";
    for (size_t i = 0; i < names.size(); i++) {
        auto scn = SnakeCaseTypeName(names[i]);
        out << "// " << names[i] << std::endl;
        // clang-format off
        if (globalList[i]) {
    out << "SP_EXPORT void *sp_ecs_set_" << scn << "(TecsLock *dynLockPtr, sp_" << scn << "_t componentPtr) {"                      << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                         << std::endl;
    out << "    Assertf(dynLock, \"sp_ecs_set_" << scn << "() called with null lock\");"                                                << std::endl;
    out << "    auto lock1 = dynLock->TryLock<Tecs::AddRemove>();"                                                                      << std::endl;
    out << "    if (lock1) {"                                                                                                           << std::endl;
    out << "        return &lock1->Set<" << names[i] << ">();"                                                                          << std::endl;
    out << "    }"                                                                                                                      << std::endl;
    out << "    auto lock2 = dynLock->TryLock<Tecs::Write<" << names[i] << ">>();"                                                      << std::endl;
    out << "    Assertf(lock2, \"Lock does not have " << names[i] << " write permissions\");"                                           << std::endl;
    out << "    return &lock2->Set<" << names[i] << ">();"                                                                              << std::endl;
    out << "}"                                                                                                                          << std::endl;
    out                                                                                                                                 << std::endl;
    out << "SP_EXPORT void *sp_ecs_get_" << scn << "(TecsLock *dynLockPtr, sp_" << scn << "_t componentPtr) {"                      << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                         << std::endl;
    out << "    Assertf(dynLock, \"sp_ecs_get_" << scn << "() called with null lock\");"                                                << std::endl;
    out << "    auto lock1 = dynLock->TryLock<Tecs::AddRemove>();"                                                                      << std::endl;
    out << "    if (lock1) {"                                                                                                           << std::endl;
    out << "        return &lock1->Get<" << names[i] << ">();"                                                                          << std::endl;
    out << "    }"                                                                                                                      << std::endl;
    out << "    auto lock2 = dynLock->TryLock<Tecs::Write<" << names[i] << ">>();"                                                      << std::endl;
    out << "    Assertf(lock2, \"Lock does not have " << names[i] << " write permissions\");"                                           << std::endl;
    out << "    return &lock2->Get<" << names[i] << ">();"                                                                              << std::endl;
    out << "}"                                                                                                                          << std::endl;
    out                                                                                                                                 << std::endl;
    out << "SP_EXPORT const void *sp_ecs_get_const_" << scn << "(TecsLock *dynLockPtr, sp_" << scn << "_t componentPtr) {"          << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                         << std::endl;
    out << "    Assertf(dynLock, \"sp_ecs_get_const_" << scn << "() called with null lock\");"                                          << std::endl;
    out << "    auto lock = dynLock->TryLock<Tecs::Read<" << names[i] << ">>();"                                                        << std::endl;
    out << "    Assertf(lock, \"Lock does not have " << names[i] << " read permissions\");"                                             << std::endl;
    out << "    return &lock->Get<const " << names[i] << ">();"                                                                         << std::endl;
    out << "}"                                                                                                                          << std::endl;
    out                                                                                                                                 << std::endl;
    out << "SP_EXPORT void sp_ecs_unset_" << scn << "(TecsLock *dynLockPtr, sp_" << scn << "_t componentPtr) {"                     << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                         << std::endl;
    out << "    Assertf(dynLock, \"sp_ecs_unset_" << scn << "() called with null lock\");"                                              << std::endl;
    out << "    auto lock = dynLock->TryLock<Tecs::AddRemove>();"                                                                       << std::endl;
    out << "    Assertf(lock, \"Lock does not have AddRemove permissions\");"                                                           << std::endl;
    out << "    lock->Unset<" << names[i] << ">();"                                                                                     << std::endl;
    out << "}"                                                                                                                          << std::endl;
    out                                                                                                                                 << std::endl;
        } else {
    out << "SP_EXPORT void *sp_entity_set_" << scn << "(TecsLock *dynLockPtr, sp_" << scn << "_t componentPtr, sp_entity_t ent) {"  << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                         << std::endl;
    out << "    Assertf(dynLock, \"sp_entity_set_" << scn << "() called with null lock\");"                                             << std::endl;
    out << "    auto lock1 = dynLock->TryLock<Tecs::AddRemove>();"                                                                      << std::endl;
    out << "    if (lock1) {"                                                                                                           << std::endl;
    out << "        return &Tecs::Entity(ent).Set<" << names[i] << ">(*lock1);"                                                         << std::endl;
    out << "    }"                                                                                                                      << std::endl;
    out << "    auto lock2 = dynLock->TryLock<Tecs::Write<" << names[i] << ">>();"                                                      << std::endl;
    out << "    Assertf(lock2, \"Lock does not have " << names[i] << " write permissions\");"                                           << std::endl;
    out << "    return &Tecs::Entity(ent).Set<" << names[i] << ">(*lock2);"                                                             << std::endl;
    out << "}"                                                                                                                          << std::endl;
    out                                                                                                                                 << std::endl;
    out << "SP_EXPORT void *sp_entity_get_" << scn << "(TecsLock *dynLockPtr, sp_" << scn << "_t componentPtr, sp_entity_t ent) {"  << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                         << std::endl;
    out << "    Assertf(dynLock, \"sp_entity_get_" << scn << "() called with null lock\");"                                             << std::endl;
    out << "    auto lock1 = dynLock->TryLock<Tecs::AddRemove>();"                                                                      << std::endl;
    out << "    if (lock1) {"                                                                                                           << std::endl;
    out << "        return &Tecs::Entity(ent).Get<" << names[i] << ">(*lock1);"                                                         << std::endl;
    out << "    }"                                                                                                                      << std::endl;
    out << "    auto lock2 = dynLock->TryLock<Tecs::Write<" << names[i] << ">>();"                                                      << std::endl;
    out << "    Assertf(lock2, \"Lock does not have " << names[i] << " write permissions\");"                                           << std::endl;
    out << "    return &Tecs::Entity(ent).Get<" << names[i] << ">(*lock2);"                                                             << std::endl;
    out << "}"                                                                                                                          << std::endl;
    out                                                                                                                                 << std::endl;
    out << "SP_EXPORT const void *sp_entity_get_const_" << scn << "(TecsLock *dynLockPtr, sp_" << scn << "_t componentPtr, sp_entity_t ent) {" << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                         << std::endl;
    out << "    Assertf(dynLock, \"sp_entity_get_const_" << scn << "() called with null lock\");"                                       << std::endl;
    out << "    auto lock = dynLock->TryLock<Tecs::Read<" << names[i] << ">>();"                                                        << std::endl;
    out << "    Assertf(lock, \"Lock does not have " << names[i] << " read permissions\");"                                             << std::endl;
    out << "    return &Tecs::Entity(ent).Get<const " << names[i] << ">(*lock);"                                                        << std::endl;
    out << "}"                                                                                                                          << std::endl;
    out                                                                                                                                 << std::endl;
    out << "SP_EXPORT void sp_entity_unset_" << scn << "(TecsLock *dynLockPtr, sp_" << scn << "_t componentPtr, sp_entity_t ent) {" << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                         << std::endl;
    out << "    Assertf(dynLock, \"sp_entity_unset_" << scn << "() called with null lock\");"                                           << std::endl;
    out << "    auto lock = dynLock->TryLock<Tecs::AddRemove>();"                                                                       << std::endl;
    out << "    Assertf(lock, \"Lock does not have AddRemove permissions\");"                                                           << std::endl;
    out << "    Tecs::Entity(ent).Unset<" << names[i] << ">(*lock);"                                                                    << std::endl;
    out << "}"                                                                                                                          << std::endl;
    out                                                                                                                                 << std::endl;
        }
        // clang-format off
    }

    out << R"RAWSTR(;
} // extern "C"
)RAWSTR";
}
