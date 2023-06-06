#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#


# No need to build the CXXOPTS tests
set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "Disable CXXOPTS Test Suite" FORCE)
set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "Disable CXXOPTS Examples" FORCE)
add_subdirectory(cxxopts)
