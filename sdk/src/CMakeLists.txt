#
# Stray Photons - Copyright (C) 2026 Jacob Wirth
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

target_sources(${PROJECT_SDK_LIB} PRIVATE
    DispatchQueue.cc
    LockFreeMutex.cc
    Logging.cc
    Utility.cc
)
