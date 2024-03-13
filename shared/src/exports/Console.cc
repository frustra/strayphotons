/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "console/Console.hh"

#include "console/CVar.hh"

#include <strayphotons.h>
#include <strayphotons/export.h>

using namespace sp;

SP_EXPORT sp_cvar_t *sp_get_cvar(const char *name) {
    return GetConsoleManager().GetCVarBase(name);
}

SP_EXPORT bool sp_cvar_get_bool(sp_cvar_t *cvar) {
    Assertf(cvar != nullptr, "sp_cvar_get_bool called with null cvar");
    auto *derived = dynamic_cast<CVar<bool> *>(cvar);
    Assertf(derived, "CVar %s has unexpected type", cvar->GetName());
    return derived->Get();
}
SP_EXPORT void sp_cvar_set_bool(sp_cvar_t *cvar, bool value) {
    Assertf(cvar != nullptr, "sp_cvar_set_bool called with null cvar");
    auto *derived = dynamic_cast<CVar<bool> *>(cvar);
    Assertf(derived, "CVar %s has unexpected type", cvar->GetName());
    derived->Set(value);
}

SP_EXPORT float sp_cvar_get_float(sp_cvar_t *cvar) {
    Assertf(cvar != nullptr, "sp_cvar_get_float called with null cvar");
    auto *derived = dynamic_cast<CVar<float> *>(cvar);
    Assertf(derived, "CVar %s has unexpected type", cvar->GetName());
    return derived->Get();
}
SP_EXPORT void sp_cvar_set_float(sp_cvar_t *cvar, float value) {
    Assertf(cvar != nullptr, "sp_cvar_set_float called with null cvar");
    auto *derived = dynamic_cast<CVar<float> *>(cvar);
    Assertf(derived, "CVar %s has unexpected type", cvar->GetName());
    derived->Set(value);
}

SP_EXPORT uint32_t sp_cvar_get_uint32(sp_cvar_t *cvar) {
    Assertf(cvar != nullptr, "sp_cvar_get_uint32 called with null cvar");
    auto *derived = dynamic_cast<CVar<uint32_t> *>(cvar);
    Assertf(derived, "CVar %s has unexpected type", cvar->GetName());
    return derived->Get();
}
SP_EXPORT void sp_cvar_set_uint32(sp_cvar_t *cvar, uint32_t value) {
    Assertf(cvar != nullptr, "sp_cvar_set_uint32 called with null cvar");
    auto *derived = dynamic_cast<CVar<uint32_t> *>(cvar);
    Assertf(derived, "CVar %s has unexpected type", cvar->GetName());
    derived->Set(value);
}

SP_EXPORT void sp_cvar_get_ivec2(sp_cvar_t *cvar, int *out_x, int *out_y) {
    Assertf(cvar != nullptr, "sp_cvar_get_ivec2 called with null cvar");
    Assertf(out_x != nullptr && out_y != nullptr, "sp_cvar_get_ivec2 called with null output ptr");
    auto *derived = dynamic_cast<CVar<glm::ivec2> *>(cvar);
    Assertf(derived, "CVar %s has unexpected type", cvar->GetName());
    auto value = derived->Get();
    *out_x = value.x;
    *out_y = value.y;
}
SP_EXPORT void sp_cvar_set_ivec2(sp_cvar_t *cvar, int value_x, int value_y) {
    Assertf(cvar != nullptr, "sp_cvar_set_ivec2 called with null cvar");
    auto *derived = dynamic_cast<CVar<glm::ivec2> *>(cvar);
    Assertf(derived, "CVar %s has unexpected type", cvar->GetName());
    derived->Set(glm::ivec2(value_x, value_y));
}
