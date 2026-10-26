#
# Stray Photons - Copyright (C) 2026 Jacob Wirth & Justine Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

set(BUILD_EXAMPLE OFF CACHE INTERNAL "")

add_subdirectory(libnyquist)


if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

    target_compile_options(libnyquist PRIVATE /O3 /wd4028 /wd4113 /wd4245 /wd4456 /wd4514 /wd4701 /wd4702 /wd4706)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(libwavpack PRIVATE $<$<COMPILE_LANGUAGE:C>:-O3 -Wno-pedantic -Wno-unused-variable -Wno-missing-braces -Wno-use-after-free -Wno-stringop-truncation -Wno-empty-translation-unit>)
    target_compile_options(libnyquist PRIVATE $<$<COMPILE_LANGUAGE:C>:-O3 -Wno-pedantic -Wno-error -Wno-stringop-overread -Wno-stringop-overflow -Wno-unused-variable -Wno-maybe-uninitialized  -Wno-sign-compare -Wno-unused-but-set-variable -Wno-unused-function -Wno-misleading-indentation -Wno-\#warnings>)
    target_compile_options(libnyquist PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-O3 -Wno-reorder -Wno-unused-variable -Wno-sign-compare -Wno-range-loop-construct>)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    target_compile_options(libwavpack PRIVATE $<$<COMPILE_LANGUAGE:C>:-O3 -Wno-unused-variable -Wno-missing-braces -Wno-empty-translation-unit>)
    target_compile_options(libnyquist PRIVATE $<$<COMPILE_LANGUAGE:C>:-O3 -Wno-shift-negative-value -Wno-macro-redefined -Wno-keyword-macro -Wno-static-in-inline -Wno-unused-but-set-variable -Wno-unused-function -Wno-misleading-indentation -Wno-\#warnings>)
    target_compile_options(libnyquist PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-O3 -Wno-reorder-ctor -Wno-unused-private-field -Wno-range-loop-construct -Wno-tautological-overlap-compare -Wno-invalid-utf8>)
endif()
