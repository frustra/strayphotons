#pragma once

#include "gen_common.hh"
#include "gen_types.hh"

#include <ranges>

template<typename S>
void GenerateComponentsH(S &out) {
    out << R"RAWSTR(#pragma once

#include <c_abi/Tecs.h>
#include <strayphotons/entity.h>
#include <strayphotons/export.h>

#if !defined(__cplusplus) || !defined(SP_SHARED_INTERNAL)
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
#else

#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cstdint>
#include <robin_hood.h>

extern "C" {
#pragma pack(push, 1)
)RAWSTR";
    ForEachComponentType([&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;
        auto &comp = ecs::LookupComponent<T>();
        GenerateCppTypeDefinition(out, comp.metadata.type);
    });
    out << R"RAWSTR(
#pragma pack(pop)
} // extern "C"
#endif
)RAWSTR";
}

template<typename S>
void GenerateComponentsCC(S &out) {
    out << R"RAWSTR(#include <strayphotons/components.h>
#include <ecs/EcsImpl.hh>

using DynamicLock = Tecs::DynamicLock<ecs::ECS>;

extern "C" {

)RAWSTR";
    ForEachComponentType([&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;
        std::string name = TypeToString<T>();
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
    out << "SP_EXPORT " << full << " *sp_ecs_set_" << scn << "(tecs_lock_t *dynLockPtr) {"                                                     << std::endl;
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
    out << "SP_EXPORT " << full << " *sp_ecs_get_" << scn << "(tecs_lock_t *dynLockPtr) {"                                                     << std::endl;
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
    out << "SP_EXPORT const " << full << " *sp_ecs_get_const_" << scn << "(tecs_lock_t *dynLockPtr) {"                                         << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                             << std::endl;
    out << "    Assertf(dynLock, \"sp_ecs_get_const_" << scn << "() called with null lock\");"                                              << std::endl;
    out << "    auto lock = dynLock->TryLock<Tecs::Read<" << name << ">>();"                                                                << std::endl;
    out << "    Assertf(lock, \"Lock does not have " << name << " read permissions\");"                                                     << std::endl;
    out << "    return reinterpret_cast<const " << full << " *>(&lock->Get<const " << name << ">());"                                       << std::endl;
    out << "}"                                                                                                                              << std::endl;
    out                                                                                                                                     << std::endl;
    out << "SP_EXPORT void sp_ecs_unset_" << scn << "(tecs_lock_t *dynLockPtr) {"                                                              << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                             << std::endl;
    out << "    Assertf(dynLock, \"sp_ecs_unset_" << scn << "() called with null lock\");"                                                  << std::endl;
    out << "    auto lock = dynLock->TryLock<Tecs::AddRemove>();"                                                                           << std::endl;
    out << "    Assertf(lock, \"Lock does not have AddRemove permissions\");"                                                               << std::endl;
    out << "    lock->Unset<" << name << ">();"                                                                                             << std::endl;
    out << "}"                                                                                                                              << std::endl;
    out                                                                                                                                     << std::endl;
        } else {
    out << "SP_EXPORT " << full << " *sp_entity_set_" << scn << "(tecs_lock_t *dynLockPtr, sp_entity_t ent) {"                                 << std::endl;
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
    out << "SP_EXPORT " << full << " *sp_entity_get_" << scn << "(tecs_lock_t *dynLockPtr, sp_entity_t ent) {"                                 << std::endl;
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
    out << "SP_EXPORT const " << full << " *sp_entity_get_const_" << scn << "(tecs_lock_t *dynLockPtr, sp_entity_t ent) {"                     << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                             << std::endl;
    out << "    Assertf(dynLock, \"sp_entity_get_const_" << scn << "() called with null lock\");"                                           << std::endl;
    out << "    auto lock = dynLock->TryLock<Tecs::Read<" << name << ">>();"                                                                << std::endl;
    out << "    Assertf(lock, \"Lock does not have " << name << " read permissions\");"                                                     << std::endl;
    out << "    return reinterpret_cast<const " << full << " *>(&Tecs::Entity(ent).Get<const " << name << ">(*lock));"                      << std::endl;
    out << "}"                                                                                                                              << std::endl;
    out                                                                                                                                     << std::endl;
    out << "SP_EXPORT void sp_entity_unset_" << scn << "(tecs_lock_t *dynLockPtr, sp_entity_t ent) {"                                          << std::endl;
    out << "    DynamicLock *dynLock = static_cast<DynamicLock *>(dynLockPtr);"                                                             << std::endl;
    out << "    Assertf(dynLock, \"sp_entity_unset_" << scn << "() called with null lock\");"                                               << std::endl;
    out << "    auto lock = dynLock->TryLock<Tecs::AddRemove>();"                                                                           << std::endl;
    out << "    Assertf(lock, \"Lock does not have AddRemove permissions\");"                                                               << std::endl;
    out << "    Tecs::Entity(ent).Unset<" << name << ">(*lock);"                                                                            << std::endl;
    out << "}"                                                                                                                              << std::endl;
    out                                                                                                                                     << std::endl;
        }
        // clang-format on
        GenerateCppTypeFunctionImplementations<T>(out, full);
    });
    for (auto &type : ReferencedCTypes()) {
        if (type == typeid(void)) continue;
        ecs::GetFieldType(type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;
            std::string scn = SnakeCaseTypeName(TypeToString<T>());
            if (sp::starts_with(scn, "ecs_")) {
                scn = scn.substr("ecs_"s.size());
            }
            std::string full = "sp_" + scn + "_t";
            GenerateCppTypeFunctionImplementations<T>(out, full);
        });
    }

    out << "} // extern \"C\"" << std::endl;
}
