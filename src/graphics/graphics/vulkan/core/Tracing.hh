#pragma once

#include <core/Tracing.hh>
#include <tracy/TracyVulkan.hpp>

#ifndef TRACY_ENABLE

    #define GPUZone(device, commandContext, name)
    #define GPUZoneNamed(device, commandContext, varname, name)
    #define GPUZoneTransient(device, commandContext, varname, name, nameLen)

#else

    #define GPUZone(device, commandContext, name) \
        TracyVkZone(device->GetTracyContext((commandContext)->GetType()), (commandContext)->Raw(), name)

    // Used when there are multiple GPU zones in the same scope
    #define GPUZoneNamed(device, commandContext, varname, name)                \
        TracyVkNamedZone(device->GetTracyContext((commandContext)->GetType()), \
            varname,                                                           \
            (commandContext)->Raw(),                                           \
            name,                                                              \
            true)

    #define GPUZoneInitializer(device, commandContext, name, nameLen)                                               \
        (device)->GetTracyContext((commandContext)->GetType()), __LINE__, __FILE__, strlen(__FILE__), __FUNCTION__, \
            strlen(__FUNCTION__), name, nameLen, (commandContext)->Raw(), true

    // Used when the name is a runtime const char *
    #define GPUZoneTransient(device, commandContext, varname, name, nameLen) \
        tracy::VkCtxScope varname(GPUZoneInitializer(device, commandContext, name, nameLen))

#endif
