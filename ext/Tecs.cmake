#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(TECS_ENABLE_PERFORMANCE_TRACING ON CACHE BOOL "" FORCE)
set(TECS_ENABLE_TRACY ${SP_ENABLE_TRACY} CACHE BOOL "" FORCE)
add_subdirectory(Tecs)
