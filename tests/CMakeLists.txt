#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

project(sp-test)

################################
# Unit test targets
################################

file(GLOB_RECURSE unit_test_sources ${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cc)
list(REMOVE_DUPLICATES unit_test_sources)

add_executable(sp-unit-tests tests.cc ${unit_test_sources})
target_compile_definitions(sp-unit-tests PRIVATE TEST_TYPE=\"unit\")
target_link_libraries(sp-unit-tests ${PROJECT_COMMON_LIB} ${PROJECT_CORE_LIB})
target_include_directories(sp-unit-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_ROOT_DIR}/include)

# target to run the tests
add_custom_target(
    unit-tests
    COMMAND sp-unit-tests
    DEPENDS sp-unit-tests
COMMENT "Run unit tests")

################################
# Integration test targets
################################

file(GLOB_RECURSE integration_test_sources ${CMAKE_CURRENT_SOURCE_DIR}/integration/*.cc)
list(REMOVE_DUPLICATES integration_test_sources)

add_executable(sp-integration-tests tests.cc ${integration_test_sources})
target_compile_definitions(sp-integration-tests PRIVATE TEST_TYPE=\"integration\")
target_link_libraries(sp-integration-tests
    ${PROJECT_COMMON_LIB}
    ${PROJECT_GAME_LIB}
    ${PROJECT_PHYSICS_PHYSX_LIB}
    ${PROJECT_SCRIPTS_LIB}
)
target_include_directories(sp-integration-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# target to run the tests
add_custom_target(
    integration-tests
    COMMAND sp-integration-tests
    DEPENDS sp-integration-tests
COMMENT "Run integration tests")
