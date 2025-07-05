/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <magic_enum.hpp>
#include <memory>
#include <string>
#include <type_traits>

#ifdef SP_SHARED_BUILD
    #include <strayphotons.h>
#endif

namespace sp::logging {
    template<typename... T>
    static void Trace(const char *, int, const std::string &, T...);
    template<typename... T>
    static void Debug(const char *, int, const std::string &, T...);
    template<typename... T>
    static void Log(const char *, int, const std::string &, T...);
    template<typename... T>
    static void Warn(const char *, int, const std::string &, T...);
    template<typename... T>
    static void Error(const char *, int, const std::string &, T...);
    template<typename... T>
    [[noreturn]] static void Abort(const char *, int, const std::string &, T...);
} // namespace sp::logging

#define Tracef(...) ::sp::logging::Trace(__FILE__, __LINE__, __VA_ARGS__)
#define Debugf(...) ::sp::logging::Debug(__FILE__, __LINE__, __VA_ARGS__)
#define Logf(...) ::sp::logging::Log(__FILE__, __LINE__, __VA_ARGS__)
#define Warnf(...) ::sp::logging::Warn(__FILE__, __LINE__, __VA_ARGS__)
#define Errorf(...) ::sp::logging::Error(__FILE__, __LINE__, __VA_ARGS__)
#define Abortf(...) ::sp::logging::Abort(__FILE__, __LINE__, __VA_ARGS__)
#define Assertf(condition, ...) \
    if (!(condition)) ::sp::logging::Abort(__FILE__, __LINE__, __VA_ARGS__)
#define Assert(condition, message) \
    if (!(condition)) Abortf("assertion failed: %s", message)

#ifdef SP_DEBUG
    #define DebugAssert(condition, message) Assert(condition, message)
    #define DebugAssertf(condition, ...) Assertf(condition, __VA_ARGS__)
#else
    #define DebugAssert(condition, message)
    #define DebugAssertf(condition, ...)
#endif

namespace sp::logging {
    enum class Level : uint8_t { Error, Warn, Log, Debug, Trace };

#ifdef SP_SHARED_BUILD
    // time in seconds
    inline float LogTime() {
        return sp_get_log_time();
    }
    inline Level GetLogLevel() {
        return sp_get_log_level();
    }
    inline void SetLogLevel(Level level) {
        sp_set_log_level(level);
    }
    inline const char *GetLogOutputFile() {
        return sp_get_log_output_file();
    }
    inline void SetLogOutputFile(const char *filePath) {
        sp_set_log_output_file(filePath);
    }
    inline void GlobalLogOutput(Level level, const string &message) {
        sp_log_message(level, message.c_str());
    }
#else
    // time in seconds
    float LogTime_static();
    Level GetLogLevel_static();
    void SetLogLevel_static(Level level);
    const char *GetLogOutputFile_static();
    void SetLogOutputFile_static(const char *filePath);
    void GlobalLogOutput_static(Level level, const string &message);

    // time in seconds
    inline float LogTime() {
        return LogTime_static();
    }
    inline Level GetLogLevel() {
        return GetLogLevel_static();
    }
    inline void SetLogLevel(Level level) {
        SetLogLevel_static(level);
    }
    inline const char *GetLogOutputFile() {
        return GetLogOutputFile_static();
    }
    inline void SetLogOutputFile(const char *filePath) {
        SetLogOutputFile_static(filePath);
    }
    inline void GlobalLogOutput(Level level, const string &message) {
        GlobalLogOutput_static(level, message);
    }
#endif

    inline static const char *basename(const char *file) {
        const char *r;
        if ((r = strrchr(file, '/'))) return r + 1;
        if ((r = strrchr(file, '\\'))) return r + 1;
        return file;
    }

    // Convert all std::strings to const char* using constexpr if (C++17)
    // Source: https://gist.github.com/Zitrax/a2e0040d301bf4b8ef8101c0b1e3f1d5
    // Modified to support string_view and enums via magic_enum.hpp
    template<typename T>
    auto convert(T &&t) {
        using BaseType = std::remove_cv_t<std::remove_reference_t<T>>;

        if constexpr (std::is_same<BaseType, std::string>()) {
            return std::forward<T>(t).c_str();
        } else if constexpr (sp::is_inline_string<BaseType>()) {
            return std::forward<T>(t).c_str();
        } else if constexpr (std::is_same<BaseType, std::string_view>()) {
            if (t.empty()) return "";
            Assert(t.data()[t.size()] == '\0', "string_view is not null terminated");
            return std::forward<T>(t).data();
        } else if constexpr (std::is_enum_v<BaseType>) {
            if (magic_enum::enum_name(std::forward<T>(t)).empty()) return "invalid_enum";
            return magic_enum::enum_name(std::forward<T>(t)).data();
        } else {
            return std::forward<T>(t);
        }
    }

    template<typename... T>
    inline static void writeFormatter(Level lvl, std::string fmt, T &&...t) {
        int size = std::snprintf(nullptr, 0, fmt.c_str(), std::forward<T>(t)...);
        // snprintf writes N + 1 bytes with null terminator
        std::string buf(size + 1, '\0');
        std::snprintf(buf.data(), size + 1, fmt.c_str(), std::forward<T>(t)...);
        buf.resize(size); // Remove unnecessary null terminator
        GlobalLogOutput(lvl, buf);
    }

    template<typename... Tn>
    inline static void writeLog(Level lvl, const char *file, int line, const std::string &fmt, Tn &&...tn) {
        writeFormatter(lvl, "%.3f " + fmt + "\n", LogTime(), convert(std::forward<Tn>(tn))...);
    }

    template<typename... T>
    static void ConsoleWrite(Level lvl, const std::string &fmt, T... t) {
        writeFormatter(lvl, fmt + "\n", convert(std::forward<T>(t))...);
    }

    template<typename... T>
    static void Trace(const char *file, int line, const std::string &fmt, T... t) {
        writeLog(Level::Trace, file, line, "[trace] " + fmt, t...);
    }

    template<typename... T>
    static void Debug(const char *file, int line, const std::string &fmt, T... t) {
        writeLog(Level::Debug, file, line, "[dbg] " + fmt, t...);
    }

    template<typename... T>
    static void Log(const char *file, int line, const std::string &fmt, T... t) {
        writeLog(Level::Log, file, line, "[log] " + fmt, t...);
    }

    template<typename... T>
    static void Warn(const char *file, int line, const std::string &fmt, T... t) {
        writeLog(Level::Warn, file, line, "[warn] " + fmt, t...);
    }

    template<typename... T>
    static void Error(const char *file, int line, const std::string &fmt, T... t) {
        writeLog(Level::Error, file, line, "[error] " + fmt, t...);
    }

    template<typename... T>
    [[noreturn]] static void Abort(const char *file, int line, const std::string &fmt, T... t) {
        writeLog(Level::Error, file, line, "[abort] " + fmt, t...);
        sp::Abort();
    }
} // namespace sp::logging

namespace sp {
    struct LogOnExit {
        const char *message;
        LogOnExit(const char *message) : message(message) {}
        ~LogOnExit() {
            if (logging::Level::Debug > logging::GetLogLevel()) return;
            std::cout << std::fixed << std::setprecision(3) << logging::LogTime() << " [debug] " << message << std::endl
                      << std::flush;
        }
    };
} // namespace sp
