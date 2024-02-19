#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

target_sources(${PROJECT_CORE_LIB} PRIVATE
    Common.cc
    DispatchQueue.cc
    LockFreeMutex.cc
    Logging.cc
    RegisteredThread.cc
)

if(TRACY_ENABLE)
    target_sources(${PROJECT_CORE_LIB} PRIVATE
        Tracing.cc
    )
endif()

# target_precompile_headers(${PROJECT_CORE_LIB} PRIVATE
#    Common.hh
# )
