#pragma once

#include "core/Logging.hh"

#include <tracy/Tracy.hpp>

#define ZoneStr(str) ZoneStrV(___tracy_scoped_zone, str)
#define ZoneStrV(varname, str) sp::tracing::TracingZoneStr(varname, str)
#define ZonePrintf(...) ZonePrintfV(___tracy_scoped_zone, __VA_ARGS__)
#define ZonePrintfV(varname, ...) sp::tracing::TracingZonePrintf(varname, __VA_ARGS__)

#define Tracef(...) sp::tracing::TracingPrintf(__VA_ARGS__)

// void *operator new(size_t size);
// void operator delete(void *ptr);

namespace sp::tracing {
    inline static void TracingZoneStr(tracy::ScopedZone &zone, const string_view &str) {
        zone.Text(str.data(), str.size());
    }

    inline static void TracingZoneStr(tracy::ScopedZone &zone, const std::string &str) {
        zone.Text(str.data(), str.size());
    }

    inline static void TracingZoneStr(tracy::ScopedZone &zone, const char *str) {
        zone.Text(str, strlen(str));
    }

    template<typename... T>
    inline static std::pair<std::unique_ptr<char[]>, int> tracingSprintf(const char *fmt, T &&...t) {
        int size = std::snprintf(nullptr, 0, fmt, std::forward<T>(t)...);
        std::unique_ptr<char[]> buf(new char[size + 1]);
        std::snprintf(buf.get(), size + 1, fmt, std::forward<T>(t)...);
        return std::make_pair(std::move(buf), size);
    }

    template<typename... T>
    inline static void TracingZonePrintf(tracy::ScopedZone &zone, const char *fmt, T &&...t) {
        auto buf = tracingSprintf(fmt, logging::convert(std::forward<T>(t))...);
        zone.Text(buf.first.get(), buf.second);
    }

    template<typename... T>
    inline static void TracingPrintf(const char *fmt, T &&...t) {
        auto buf = tracingSprintf(fmt, logging::convert(std::forward<T>(t))...);
        TracyMessage(buf.first.get(), buf.second);
    }

} // namespace sp::tracing
