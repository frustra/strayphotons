#pragma once

#include "core/Common.hh"
#include "core/LockFreeMutex.hh"
#include "core/PreservingMap.hh"

#include <ankerl/unordered_dense.h>
#include <memory>

namespace ecs {
    typedef std::shared_ptr<std::string> StringHandle;

    class StringHandleManager {
    public:
        StringHandleManager() {}

        StringHandle Get(const std::string &str);

        void Tick(chrono_clock::duration maxTickInterval);

    private:
        sp::LockFreeMutex mutex;
        sp::PreservingMap<std::string, std::string> strings;
    };

    StringHandleManager &GetStringHandler();
} // namespace ecs
