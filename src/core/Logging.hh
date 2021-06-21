#pragma once

// #define SP_VERBOSE_LOGGING
#include "Common.hh"

#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#define Debugf(...) sp::logging::Debug(__FILE__, __LINE__, __VA_ARGS__)
#define Logf(...) sp::logging::Log(__FILE__, __LINE__, __VA_ARGS__)
#define Errorf(...) sp::logging::Error(__FILE__, __LINE__, __VA_ARGS__)

namespace sp {
    namespace logging {
        enum class Level { Error, Log, Debug };

        void GlobalLogOutput(Level lvl, const std::string &line);

        inline static const char *basename(const char *file) {
            const char *r;
            if ((r = strrchr(file, '/'))) return r + 1;
            if ((r = strrchr(file, '\\'))) return r + 1;
            return file;
        }

        // Convert all std::strings to const char* using constexpr if (C++17)
        // Source: https://gist.github.com/Zitrax/a2e0040d301bf4b8ef8101c0b1e3f1d5
        template<typename T>
        auto convert(T &&t) {
            if constexpr (std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, std::string>::value) {
                return std::forward<T>(t).c_str();
            } else {
                return std::forward<T>(t);
            }
        }

        template<typename... T>
        inline static void writeFormatter(Level lvl, const std::string &fmt, T &&...t) {
            // #ifdef PACKAGE_RELEASE
            if (lvl == logging::Level::Debug) return;
            // #endif
            int size = std::snprintf(nullptr, 0, fmt.c_str(), std::forward<T>(t)...);
            std::unique_ptr<char[]> buf(new char[size + 1]);
            std::snprintf(buf.get(), size + 1, fmt.c_str(), std::forward<T>(t)...);
            std::cerr << buf.get();

            if (lvl != logging::Level::Debug) { GlobalLogOutput(lvl, string(buf.get(), buf.get() + size)); }
        }

        template<typename T1, typename... Tn>
        inline static void writeLog(Level lvl, const char *file, int line, const std::string &fmt, T1 t1, Tn &&...tn) {
#ifdef SP_VERBOSE_LOGGING
            writeFormatter(lvl,
                           fmt + "  (%s:%d)\n",
                           convert(t1),
                           convert(std::forward<Tn>(tn))...,
                           basename(file),
                           line);
#else
            writeFormatter(lvl, fmt + "\n", convert(t1), convert(std::forward<Tn>(tn))...);
#endif
        }

        inline static void writeLog(Level lvl, const char *file, int line, const std::string &str) {
#ifdef SP_VERBOSE_LOGGING
            writeFormatter(lvl, "%s  (%s:%d)\n", convert(str), basename(file), line);
#else
            writeFormatter(lvl, "%s\n", convert(str));
#endif
        }

        template<typename... T>
        static void ConsoleWrite(Level lvl, const std::string &fmt, T... t) {
            writeFormatter(lvl, fmt + "\n", convert(std::forward<T>(t))...);
        }

        template<typename... T>
        static void Log(const char *file, int line, const std::string &fmt, T... t) {
            writeLog(Level::Log, file, line, "[log] " + fmt, t...);
        }

        template<typename... T>
        static void Debug(const char *file, int line, const std::string &fmt, T... t) {
            writeLog(Level::Debug, file, line, "[dbg] " + fmt, t...);
        }

        template<typename... T>
        static void Error(const char *file, int line, const std::string &fmt, T... t) {
            writeLog(Level::Error, file, line, "[err] " + fmt, t...);
        }
    } // namespace logging
} // namespace sp
